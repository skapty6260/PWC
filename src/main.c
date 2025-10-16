#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vulkan/vulkan_core.h>
#include <wayland-version.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <pwc/render/vulkan.h>
#include <pwc/render/drm.h>

int framebuffer_draw_screen(struct pwc_drm *drm) {
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

static int vulkandrm_draw_screen(struct pwc_drm *drm, struct pwc_vulkan *vulkan) {
    int fd; // FD для DMA-BUF
    VkImage vulkan_image;

    if (create_vulkan_image_and_export_fd(vulkan, &fd, &vulkan_image)) {
        fprintf(stderr, "Failed to create and export Vulkan image\n");
        return EXIT_FAILURE;
    }

    // Добавьте рендеринг
    if (render_red_screen_to_image(vulkan, vulkan_image) != VK_SUCCESS) {
        fprintf(stderr, "Failed to render red screen to image\n");
        close(fd);  // Очистите FD
        return EXIT_FAILURE;
    }

    struct gbm_device *gbm = gbm_create_device(drm->fd);
    if (!gbm) {
        fprintf(stderr, "Failed to create GBM device\n");
        close(fd);
        return EXIT_FAILURE;
    }

    // Import FD to GBM buffer
    struct gbm_import_fd_data import_data = {
        .fd = fd,
        .width = vulkan->width,
        .height = vulkan->height,
        .format = GBM_FORMAT_ARGB8888,
    };

    struct gbm_bo *bo = gbm_bo_import(gbm, GBM_BO_IMPORT_FD, &import_data, 0);
    if (!bo) {
        fprintf(stderr, "Failed to create gbm buffer from FD\n");
        gbm_device_destroy(gbm);
        close(fd);
        return EXIT_FAILURE;
    }

    uint32_t handle = gbm_bo_get_handle(bo).u32;
    // uint32_t stride = gbm_bo_get_stride(bo);
    uint32_t pitches, offsets, fb_id;

    if (drmModeAddFB2(drm->fd, drm->preferred_mode->hdisplay, drm->preferred_mode->vdisplay,
        GBM_FORMAT_ARGB8888, &handle, &pitches, &offsets, &fb_id, 0) != 0)
    {
        fprintf(stderr, "Failedd to add fb2 to drm device");
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbm);
        close(fd);
        return EXIT_FAILURE;
    }

    drmModeSetCrtc(drm->fd, drm->crtc->crtc_id, fb_id, 0, 0, &drm->connector->connector_id, 1, drm->preferred_mode);

    sleep(5);

    // Очистка
    drmModeRmFB(drm->fd, fb_id);
    gbm_bo_destroy(bo);
    gbm_device_destroy(gbm);
    close(fd);  // Закрываем FD

    return EXIT_SUCCESS;
};

int main(int argc, char **argv) {
    printf("PURE WAYLAND COMPOSITOR\n");
    printf("WAYLAND version: %s\n", WAYLAND_VERSION);  // Use _S for string (if defined; fallback to manual)
    printf("PWC version: 0.01dev\n");

    // Init drm
    const char *card = (argc > 1) ? argv[1] : "card0";  // Allow override, default to card0
    char path[32];
    snprintf(path, sizeof(path), "/dev/dri/%s", card);

    struct pwc_drm *drm = calloc(1, sizeof(struct pwc_drm));
    struct pwc_vulkan *vulkan = calloc(1, sizeof(struct pwc_vulkan));
    if (init_drm(drm, path, vulkan)) {
        fprintf(stderr, "Failed to init drm backend\n");
        return EXIT_FAILURE;
    }

    vulkandrm_draw_screen(drm, vulkan);
    printf("Succesfully drawed a vulkan image on screen.\n");

    framebuffer_draw_screen(drm);
    printf("Succesfully drawed via framebuffer.");

    return EXIT_SUCCESS;
}