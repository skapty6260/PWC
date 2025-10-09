/*
 * Copyright (c) 2016 Dongseong Hwang <dongseong.hwang@intel.com>
 *
 * ... (copyright as in original)
 */

#include "egl_drm_glue.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <gbm.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NUM_BUFFERS 2

// Typedefs for EGL extensions (as in original)
typedef EGLImageKHR (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC)(EGLDisplay dpy, EGLImageKHR image);
typedef void (EGLAPIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);
typedef EGLSyncKHR (EGLAPIENTRYP PFNEGLCREATESYNCKHRPROC)(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);
typedef EGLint (EGLAPIENTRYP PFNEGLCLIENTWAITSYNCKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);

// Internal EGLGlue struct
typedef struct {
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    PFNEGLCREATEIMAGEKHRPROC CreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC DestroyImageKHR;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC EGLImageTargetTexture2DOES;
    PFNEGLCREATESYNCKHRPROC CreateSyncKHR;
    PFNEGLCLIENTWAITSYNCKHRPROC ClientWaitSyncKHR;
    int egl_sync_supported;
} ged_EGLGlue;

// Internal Framebuffer struct
typedef struct {
    struct gbm_bo* bo;
    int fd;
    uint32_t fb_id;
    EGLImageKHR image;
    GLuint gl_tex;
    GLuint gl_fb;
} ged_Framebuffer;

// Internal Impl struct for EGLDRMGlue
typedef struct ged_EGLDRMGlue_Impl {
    ged_DRMModesetter* drm;
    ged_SwapBuffersCallback callback;
    void* callback_user_data;
    struct gbm_device* gbm;
    ged_EGLGlue egl;
    ged_Framebuffer framebuffers[NUM_BUFFERS];
} ged_EGLDRMGlue_Impl;

/*
 * Copyright (c) 2016 Dongseong Hwang <dongseong.hwang@intel.com>
 *
 * ... (copyright continues as before)
 */

// ... (includes, typedefs, and internal structs as in previous response)

static const char* ged_egl_get_error(void) {
    EGLint error = eglGetError();
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "EGL_???";
    }
}

static int ged_extensions_contain(const char* name, const char* extensions) {
    if (!name || !extensions) return 0;
    char* ext_copy = strdup(extensions);
    if (!ext_copy) return 0;
    strcat(ext_copy, " ");

    char* delimited_name = malloc(strlen(name) + 2);
    if (!delimited_name) {
        free(ext_copy);
        return 0;
    }
    strcpy(delimited_name, name);
    strcat(delimited_name, " ");

    char* found = strstr(ext_copy, delimited_name);
    free(delimited_name);
    free(ext_copy);
    return found != NULL;
}

// StreamTextureImpl (internal implementation)
typedef struct {
    ged_StreamTexture base;
    const ged_EGLGlue* egl;
    struct gbm_bo* bo;
    int fd;
    EGLImageKHR image;
    GLuint gl_tex;
    ged_StreamTexture_Dimension dimension;
    void* addr;
} ged_StreamTextureImpl;

static void* ged_stream_texture_impl_map(ged_StreamTexture* base) {
    ged_StreamTextureImpl* self = (ged_StreamTextureImpl*)base->user_data;
    assert(self->addr == NULL);
    size_t size = (size_t)self->dimension.stride * (size_t)self->dimension.height;
    self->addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, self->fd, 0);
    if (self->addr == MAP_FAILED) {
        self->addr = NULL;
        return NULL;
    }
    return self->addr;
}

static void ged_stream_texture_impl_unmap(ged_StreamTexture* base) {
    ged_StreamTextureImpl* self = (ged_StreamTextureImpl*)base->user_data;
    assert(self->addr != NULL);
    size_t size = (size_t)self->dimension.stride * (size_t)self->dimension.height;
    munmap(self->addr, size);
    self->addr = NULL;
}

static GLuint ged_stream_texture_impl_get_texture_id(const ged_StreamTexture* base) {
    const ged_StreamTextureImpl* self = (const ged_StreamTextureImpl*)base->user_data;
    return self->gl_tex;
}

static ged_StreamTexture_Dimension ged_stream_texture_impl_get_dimension(const ged_StreamTexture* base) {
    const ged_StreamTextureImpl* self = (const ged_StreamTextureImpl*)base->user_data;
    return self->dimension;
}

static void ged_stream_texture_impl_destroy(ged_StreamTexture* base) {
    if (!base || !base->user_data) return;
    ged_StreamTextureImpl* self = (ged_StreamTextureImpl*)base->user_data;
    glDeleteTextures(1, &self->gl_tex);
    if (self->egl && self->egl->DestroyImageKHR) {
        self->egl->DestroyImageKHR(self->egl->display, self->image);
    }
    if (self->fd >= 0) close(self->fd);
    if (self->bo) gbm_bo_destroy(self->bo);
    if (self->addr) munmap(self->addr, (size_t)self->dimension.stride * (size_t)self->dimension.height);
    free(base->user_data);
    free(base);
}

static ged_StreamTexture* ged_stream_texture_impl_create(struct gbm_device* gbm, const ged_EGLGlue* egl, size_t width, size_t height) {
    ged_StreamTexture* base = (ged_StreamTexture*)malloc(sizeof(ged_StreamTexture));
    if (!base) return NULL;
    memset(base, 0, sizeof(ged_StreamTexture));

    ged_StreamTextureImpl* impl = (ged_StreamTextureImpl*)malloc(sizeof(ged_StreamTextureImpl));
    if (!impl) {
        free(base);
        return NULL;
    }
    memset(impl, 0, sizeof(ged_StreamTextureImpl));
    base->user_data = impl;

    impl->egl = egl;
    impl->dimension.width = (int)width;
    impl->dimension.height = (int)height;

    impl->bo = gbm_bo_create(gbm, impl->dimension.width, impl->dimension.height, GBM_FORMAT_ARGB8888, GBM_BO_USE_LINEAR);
    if (!impl->bo) {
        fprintf(stderr, "failed to create a gbm buffer.\n");
        ged_stream_texture_impl_destroy(base);
        return NULL;
    }

    impl->fd = gbm_bo_get_fd(impl->bo);
    if (impl->fd < 0) {
        fprintf(stderr, "failed to get fd for bo: %d\n", impl->fd);
        ged_stream_texture_impl_destroy(base);
        return NULL;
    }

    impl->dimension.stride = (int)gbm_bo_get_stride(impl->bo);
    EGLint offset = 0;
    const EGLint khr_image_attrs[] = {
        EGL_DMA_BUF_PLANE0_FD_EXT, impl->fd,
        EGL_WIDTH, impl->dimension.width,
        EGL_HEIGHT, impl->dimension.height,
        EGL_LINUX_DRM_FOURCC_EXT, GBM_FORMAT_ARGB8888,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, impl->dimension.stride,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
        EGL_NONE
    };

    impl->image = egl->CreateImageKHR(egl->display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, khr_image_attrs);
    if (impl->image == EGL_NO_IMAGE_KHR) {
        fprintf(stderr, "failed to make image from buffer object: %s\n", ged_egl_get_error());
        ged_stream_texture_impl_destroy(base);
        return NULL;
    }

    glGenTextures(1, &impl->gl_tex);
    glBindTexture(GL_TEXTURE_2D, impl->gl_tex);
    egl->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, impl->image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Set vtable
    base->vtable.map = ged_stream_texture_impl_map;
    base->vtable.unmap = ged_stream_texture_impl_unmap;
    base->vtable.get_texture_id = ged_stream_texture_impl_get_texture_id;
    base->vtable.get_dimension = ged_stream_texture_impl_get_dimension;

    return base;
}

// EGLGlue initialization
static int ged_egl_glue_initialize(ged_EGLGlue* egl) {
    egl->CreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    egl->DestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    egl->EGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    egl->CreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    egl->ClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");

    if (!egl->CreateImageKHR || !egl->DestroyImageKHR || !egl->EGLImageTargetTexture2DOES) {
        fprintf(stderr, "eglGetProcAddress returned NULL for a required extension entry point.\n");
        return 0;
    }

    if (egl->CreateSyncKHR && egl->ClientWaitSyncKHR) {
        egl->egl_sync_supported = 1;
    } else {
        egl->egl_sync_supported = 0;
    }

    egl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    EGLint major, minor;
    if (!eglInitialize(egl->display, &major, &minor)) {
        fprintf(stderr, "failed to initialize\n");
        return 0;
    }

    printf("Using display %p with EGL version %d.%d\n", (void*)egl->display, major, minor);
    printf("EGL Version \"%s\"\n", eglQueryString(egl->display, EGL_VERSION));
    printf("EGL Vendor \"%s\"\n", eglQueryString(egl->display, EGL_VENDOR));

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(stderr, "failed to bind api EGL_OPENGL_ES_API\n");
        return 0;
    }

    static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_DONT_CARE, EGL_NONE};
    EGLint num_config = 0;
    if (!eglChooseConfig(egl->display, config_attribs, &egl->config, 1, &num_config) || num_config != 1) {
        fprintf(stderr, "failed to choose config: %d\n", num_config);
        return 0;
    }

    static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    egl->context = eglCreateContext(egl->display, egl->config, EGL_NO_CONTEXT, context_attribs);
    if (egl->context == EGL_NO_CONTEXT) {
        fprintf(stderr, "failed to create context\n");
        return 0;
    }

    if (!eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl->context)) {
        fprintf(stderr, "failed to make the OpenGL ES Context current: %s\n", ged_egl_get_error());
        return 0;
    }

    const char* egl_extensions = (const char*)eglQueryString(egl->display, EGL_EXTENSIONS);
    printf("EGL Extensions \"%s\"\n", egl_extensions);
    if (!ged_extensions_contain("EGL_KHR_image_base", egl_extensions)) {
        fprintf(stderr, "EGL_KHR_image_base extension not supported\n");
        return 0;
    }
    if (!ged_extensions_contain("EGL_EXT_image_dma_buf_import", egl_extensions)) {
        fprintf(stderr, "EGL_EXT_image_dma_buf_import extension not supported\n");
        return 0;
    }

    const char* gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (!ged_extensions_contain("GL_OES_EGL_image", gl_extensions)) {
        fprintf(stderr, "GL_OES_EGL_image extension not supported\n");
        return 0;
    }

    return 1;
}

static void ged_egl_glue_sync_fence(ged_EGLGlue* egl) {
    if (egl->egl_sync_supported) {
        EGLSyncKHR sync = egl->CreateSyncKHR(egl->display, EGL_SYNC_FENCE_KHR, NULL);
        glFlush();
        egl->ClientWaitSyncKHR(egl->display, sync, 0, EGL_FOREVER_KHR);
        // Note: Destroy sync if needed, but original doesn't
    } else {
        glFinish();
    }
}

static int ged_egl_glue_create_framebuffer(struct gbm_device* gbm, ged_EGLGlue* egl, int width, int height, ged_Framebuffer* fb) {
    memset(fb, 0, sizeof(ged_Framebuffer));
    fb->bo = gbm_bo_create(gbm, width, height, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!fb->bo) {
        fprintf(stderr, "failed to create a gbm buffer.\n");
        return 0;
    }

    fb->fd = gbm_bo_get_fd(fb->bo);
    if (fb->fd < 0) {
        fprintf(stderr, "failed to get fd for bo: %d\n", fb->fd);
        return 0;
    }

    uint32_t handles[4] = {gbm_bo_get_handle(fb->bo).u32, 0, 0, 0};
    uint32_t pitches[4] = {gbm_bo_get_stride(fb->bo), 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};
    uint32_t ret = drmModeAddFB2(gbm_bo_get_device(gbm)->fd, width, height, GBM_FORMAT_XRGB8888, handles, pitches, offsets, &fb->fb_id, 0);
    if (!fb->fb_id) {
        fprintf(stderr, "failed to create framebuffer from buffer object.\n");
        return 0;
    }

    const EGLint khr_image_attrs[] = {
        EGL_DMA_BUF_PLANE0_FD_EXT, fb->fd,
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_LINUX_DRM_FOURCC_EXT, GBM_FORMAT_XRGB8888,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)pitches[0],
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, (EGLint)offsets[0],
        EGL_NONE
    };

    fb->image = egl->CreateImageKHR(egl->display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, khr_image_attrs);
    if (fb->image == EGL_NO_IMAGE_KHR) {
        fprintf(stderr, "failed to make image from buffer object: %s\n", ged_egl_get_error());
        return 0;
    }

    glGenTextures(1, &fb->gl_tex);
    glBindTexture(GL_TEXTURE_2D, fb->gl_tex);
    egl->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, fb->image);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &fb->gl_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb->gl_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->gl_tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "failed framebuffer check for created target buffer: 0x%x\n", status);
        glDeleteFramebuffers(1, &fb->gl_fb);
        glDeleteTextures(1, &fb->gl_tex);
        return 0;
    }

    return 1;
}

// DRMModesetter Client implementation for Impl
static void ged_egl_drm_glue_impl_did_page_flip(int front_buffer, unsigned int sec, unsigned int usec, void* user_data) {
    ged_EGLDRMGlue_Impl* impl = (ged_EGLDRMGlue_Impl*)user_data;
    ged_Framebuffer* back_fb = &impl->framebuffers[front_buffer ^ 1];

    glBindFramebuffer(GL_FRAMEBUFFER, back_fb->gl_fb);
    if (impl->callback) {
        impl->callback(back_fb->gl_fb, (unsigned long)(sec * 1000000ULL + usec), impl->callback_user_data);
    }
    ged_egl_glue_sync_fence(&impl->egl);
}

static uint32_t ged_egl_drm_glue_impl_get_frame_buffer(int front_buffer, void* user_data) {
    ged_EGLDRMGlue_Impl* impl = (ged_EGLDRMGlue_Impl*)user_data;
    return impl->framebuffers[front_buffer].fb_id;
}

static ged_DRMModesetter_Client ged_egl_drm_glue_client = {
    .vtable = {
        .did_page_flip = ged_egl_drm_glue_impl_did_page_flip,
        .get_frame_buffer = ged_egl_drm_glue_impl_get_frame_buffer
    },
    .user_data = NULL  // Set later
};

static int ged_egl_drm_glue_impl_initialize(ged_EGLDRMGlue_Impl* impl) {
    impl->gbm = gbm_create_device(ged_drm_modesetter_get_fd(impl->drm));
    if (!impl->gbm) {
        fprintf(stderr, "cannot create gbm device.\n");
        return 0;
    }

    if (!ged_egl_glue_initialize(&impl->egl)) {
        fprintf(stderr, "cannot create EGL context.\n");
        return 0;
    }

    ged_DRMModesetter_Size display_size = ged_drm_modesetter_get_display_size(impl->drm);
    int i;
    for (i = 0; i < NUM_BUFFERS; ++i) {
        if (!ged_egl_glue_create_framebuffer(impl->gbm, &impl->egl, display_size.width, display_size.height, &impl->framebuffers[i])) {
            fprintf(stderr, "cannot create framebuffer.\n");
            return 0;
        }
   