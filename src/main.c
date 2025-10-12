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

#include <pwc/vulkan.h>
#include <pwc/drm.h>

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

    // Draw screen
    // if (framebuffer_draw_screen(drm)) {
    //     fprintf(stderr, "Failed to draw screen with framebuffer\n");
    //     return EXIT_FAILURE;
    // }

    return EXIT_SUCCESS;
}