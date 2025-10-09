#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <wayland-version.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

drmModeConnectorPtr getFirstConnectedConnector(drmModeResPtr mode_resources, int fd) {
    printf("Num connectors: %d\n", mode_resources->count_connectors);  // Debug: Print count

    for (int i = 0; i < mode_resources->count_connectors; i++) {
        drmModeConnectorPtr connector = drmModeGetConnector(fd, mode_resources->connectors[i]);
        if (!connector) {
            printf("  Connector %d: Failed to get\n", i);  // Debug
            continue;
        }

        printf("  Connector %d: Type %d:%d, Connection status %d (%s)\n",
               i, connector->connector_type, connector->connector_type_id,
               connector->connection,
               (connector->connection == DRM_MODE_CONNECTED ? "Connected" :
                connector->connection == DRM_MODE_DISCONNECTED ? "Disconnected" :
                "Unknown"));  // Debug: Status per connector

        if (connector->connection == DRM_MODE_CONNECTED) {
            return connector;  // Found: Return without freeing
        }

        drmModeFreeConnector(connector);
    }

    return NULL;  // None connected
}

drmModeModeInfoPtr getPreferredMode(drmModeConnectorPtr connector, int fd) {
    for (int i = 0; i < connector->count_modes; i++) {
        if (connector->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
            return &connector->modes[i];
        }
    }

    return NULL;
}

struct gl {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
	GLuint program;
	GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
	GLuint vbo;
	GLuint positionsoffset, colorsoffset, normalsoffset;
};

struct drm {
    int fd;

    drmModeResPtr mode_resources;
    drmModeConnectorPtr connector;
    drmModeModeInfoPtr preferred_mode;

    drmModeEncoder *encoder;
    drmModeCrtc *crtc;
};

struct gbm {
    struct gbm_device *device;
    struct gbm_surface *surface;
};

static int framebuffer_draw_screen(struct drm *drm) {
    // FrameBuffer
    uint32_t db_id, pitch;
    uint64_t size;
    if (drmModeCreateDumbBuffer(drm->fd,
        drm->preferred_mode->hdisplay,
        drm->preferred_mode->vdisplay,
        32,
        0, 
        &db_id,
        &pitch,
        &size)
    ) {
        fprintf(stderr, "Failed to create framebuffer\n");
        return EXIT_FAILURE;
    }

    uint32_t fb_id;
    if (drmModeAddFB(drm->fd,
        drm->preferred_mode->hdisplay,
        drm->preferred_mode->vdisplay,
        24,
        32,
        pitch,
        db_id, 
        &fb_id)
    ) {
        fprintf(stderr, "Failed to add framebuffer\n");
        return EXIT_FAILURE;
    }

    // Drawing
    uint64_t db_offset;
    if (drmModeMapDumbBuffer(drm->fd, db_id, &db_offset)) {
        fprintf(stderr, "Failed to prepare scanout buffer map\n");
        return EXIT_FAILURE;
    }

    uint32_t* db_data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, drm->fd, db_offset);
    memset(db_data, 0xffffffff, size);

    // Connectors are array, count is count of connectors, maybe on mirror implementation this would help.
    drmModeSetCrtc(drm->fd, drm->crtc->crtc_id, 0, 0, 0, NULL, 0, NULL);
    drmModeSetCrtc(drm->fd, drm->crtc->crtc_id, fb_id, 0, 0, &drm->connector->connector_id, 1, drm->preferred_mode);

    sleep(5);
    return EXIT_SUCCESS;
}

static int init_drm(struct drm *drm, int argc, char **argv) {
    // Open card from args
    const char *card = (argc > 1) ? argv[1] : "card0";  // Allow override, default to card0
    char path[32];
    snprintf(path, sizeof(path), "/dev/dri/%s", card);

    drm->fd = open(path, O_RDWR);
    if (drm->fd < 0) {
        fprintf(stderr, "Failed to open %s (try sudo or check device)\n", path);
        return EXIT_FAILURE;
    }
    printf("Opened DRM device: %s (fd=%d)\n", path, drm->fd);

    drm->mode_resources = drmModeGetResources(drm->fd);
    if (drm->mode_resources == NULL) {
        fprintf(stderr, "Failed to get resources\n");
        close(drm->fd);
        return EXIT_FAILURE;
    }

    printf("Num crtcs: %d\n", drm->mode_resources->count_crtcs);
    printf("Num encoders: %d\n", drm->mode_resources->count_encoders);
    printf("Num fbs: %d\n", drm->mode_resources->count_fbs);

    // Search for first connected connector
    drm->connector = getFirstConnectedConnector(drm->mode_resources, drm->fd);
    if (drm->connector == NULL) {
        fprintf(stderr, "Failed to get first connected connector\n");
        drmModeFreeResources(drm->mode_resources);
        close(drm->fd);
        return EXIT_FAILURE;
    }
    printf("Found connected connector: Type %d:%d\n", drm->connector->connector_type, drm->connector->connector_type_id);

    // Preferred monitor output mode
    drm->preferred_mode = getPreferredMode(drm->connector, drm->fd);
    if (drm->preferred_mode == NULL) {
        fprintf(stderr, "Failed to get preferred mode\n");
        drmModeFreeConnector(drm->connector);
        drmModeFreeResources(drm->mode_resources);
        close(drm->fd);
        return EXIT_FAILURE;
    }
    printf("Preferred mode:\n");
    printf("  Horizontal: display=%d, sync_start=%d, sync_end=%d, total=%d\n",
           drm->preferred_mode->hdisplay, drm->preferred_mode->hsync_start, drm->preferred_mode->hsync_end, drm->preferred_mode->htotal);
    printf("  Vertical: display=%d, sync_start=%d, sync_end=%d, total=%d\n",
           drm->preferred_mode->vdisplay, drm->preferred_mode->vsync_start, drm->preferred_mode->vsync_end, drm->preferred_mode->vtotal);
    printf("  Clock: %d kHz, Flags: 0x%x\n", drm->preferred_mode->clock / 1000, drm->preferred_mode->flags);

    // Encoder
    drm->encoder = drmModeGetEncoder(drm->fd, drm->connector->encoder_id);
    if (drm->encoder == NULL) {
        fprintf(stderr, "Failed to get drm encoder\n");
        return EXIT_FAILURE;
    }

    // CRTC
    drm->crtc = drmModeGetCrtc(drm->fd, drm->encoder->crtc_id);
    if (drm->crtc == NULL) {
        fprintf(stderr, "Failed to get drm crtc\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int init_gbm(struct gbm *gbm, int drm_fd, uint32_t width, uint32_t height) {
    gbm->device = gbm_create_device(drm_fd);
    printf("Creating gbm device on fd: %d\n", drm_fd);

    gbm->surface = gbm_surface_create(
        gbm->device,
        width,
        height,
        GBM_FORMAT_ARGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
    );

    if (gbm->surface == NULL) {
        fprintf(stderr, "failed to create gbm surface\n");
		return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


// TODO: Rework with drm
static int init_gl(struct gl *gl, struct gbm *gbm) {
    EGLint major, minor, n;
    // GLuint vertex_shader, fragment_shader;
    // GLint ret;

    static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;
	get_platform_display =
		(void *) eglGetProcAddress("eglGetPlatformDisplayEXT");
	assert(get_platform_display != NULL);

    gl->display = get_platform_display(EGL_PLATFORM_GBM_KHR, gbm->device, NULL);

	if (!eglInitialize(gl->display, &major, &minor)) {
		printf("failed to initialize\n");
		return EXIT_FAILURE;
	}

	printf("Using display %p with EGL version %d.%d\n",
			gl->display, major, minor);

	printf("EGL Version \"%s\"\n", eglQueryString(gl->display, EGL_VERSION));
	printf("EGL Vendor \"%s\"\n", eglQueryString(gl->display, EGL_VENDOR));
	printf("EGL Extensions \"%s\"\n", eglQueryString(gl->display, EGL_EXTENSIONS));

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("failed to bind api EGL_OPENGL_ES_API\n");
		return EXIT_FAILURE;
	}

	if (!eglChooseConfig(gl->display, config_attribs, &gl->config, 1, &n) || n != 1) {
		printf("failed to choose config: %d\n", n);
		return EXIT_FAILURE;
	}

	gl->context = eglCreateContext(gl->display, gl->config,
			EGL_NO_CONTEXT, context_attribs);
	if (gl->context == NULL) {
		printf("failed to create context\n");
		return EXIT_FAILURE;
	}

	gl->surface = eglCreateWindowSurface(gl->display, gl->config, gbm->surface, NULL);
	if (gl->surface == EGL_NO_SURFACE) {
		printf("Failed to create egl surface\n");
		return EXIT_FAILURE;
	}

	/* connect the context to the surface */
	eglMakeCurrent(gl->display, gl->surface, gl->surface, gl->context);

	printf("GL Extensions: \"%s\"\n", glGetString(GL_EXTENSIONS));

	return EXIT_SUCCESS;
}

static void cleanup(struct drm *drm, struct gbm *gbm, struct gl *gl) {
    drmModeFreeConnector(drm->connector);
    drmModeFreeResources(drm->mode_resources);
    close(drm->fd);
    free(drm);

    free(gbm);
    free(gl);
}

int main(int argc, char **argv) {
    printf("PURE WAYLAND COMPOSITOR\n");
    printf("WAYLAND version: %s\n", WAYLAND_VERSION);  // Use _S for string (if defined; fallback to manual)
    printf("PWC version: 0.01dev\n");

    // Init drm
    struct drm *drm = calloc(1, sizeof(struct drm));
    if (init_drm(drm, argc, argv)) {
        fprintf(stderr, "Failed to init drm\n");
        return EXIT_FAILURE;
    }
    
    // Init gbm
    struct gbm *gbm = calloc(1, sizeof(struct gbm));
    if (init_gbm(gbm, drm->fd, drm->preferred_mode->hdisplay, drm->preferred_mode->vdisplay)) {
        fprintf(stderr, "Failed to init gbm\n");
        return EXIT_FAILURE;
    }

    struct gl *gl = calloc(1, sizeof(struct gl));
    if (init_gl(gl, gbm)) {
        fprintf(stderr, "Failed to init gl\n");
        return EXIT_FAILURE;
    }

    // Draw screen
    if (framebuffer_draw_screen(drm)) {
        fprintf(stderr, "Failed to draw screen with framebuffer\n");
        return EXIT_FAILURE;
    }

    cleanup(drm, gbm, gl);
    return EXIT_SUCCESS;
}