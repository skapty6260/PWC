/**
 * @file main.c
 * @brief PWC Wayland Compositor Main Entry Point.
 *
 * Initializes compositor, globals, DRM backend, and event loop.
 * Handles permissions gracefully (warns if non-root).
 */

#include "drm_mode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <fcntl.h>
#include <wayland-version.h>
#include <xf86drmMode.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

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

int main(int argc, char **argv) {
    printf("PURE WAYLAND COMPOSITOR\n");
    printf("WAYLAND version: %s\n", WAYLAND_VERSION);  // Use _S for string (if defined; fallback to manual)
    printf("PWC version: 0.01dev\n");

    // Open card from args
    const char *card = (argc > 1) ? argv[1] : "card0";  // Allow override, default to card0
    char path[32];
    snprintf(path, sizeof(path), "/dev/dri/%s", card);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s (try sudo or check device)\n", path);
        return EXIT_FAILURE;
    }
    printf("Opened DRM device: %s (fd=%d)\n", path, fd);

    drmModeResPtr mode_resources = drmModeGetResources(fd);
    if (mode_resources == NULL) {
        fprintf(stderr, "Failed to get resources\n");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Num crtcs: %d\n", mode_resources->count_crtcs);
    printf("Num encoders: %d\n", mode_resources->count_encoders);
    printf("Num fbs: %d\n", mode_resources->count_fbs);

    // Search for first connected connector
    drmModeConnectorPtr connector = getFirstConnectedConnector(mode_resources, fd);
    if (connector == NULL) {
        fprintf(stderr, "Failed to get first connected connector\n");
        drmModeFreeResources(mode_resources);
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Found connected connector: Type %d:%d\n", connector->connector_type, connector->connector_type_id);

    // Preferred monitor output mode
    drmModeModeInfoPtr preferred_mode = getPreferredMode(connector, fd);
    if (preferred_mode == NULL) {
        fprintf(stderr, "Failed to get preferred mode\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(mode_resources);
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Preferred mode:\n");
    printf("  Horizontal: display=%d, sync_start=%d, sync_end=%d, total=%d\n",
           preferred_mode->hdisplay, preferred_mode->hsync_start, preferred_mode->hsync_end, preferred_mode->htotal);
    printf("  Vertical: display=%d, sync_start=%d, sync_end=%d, total=%d\n",
           preferred_mode->vdisplay, preferred_mode->vsync_start, preferred_mode->vsync_end, preferred_mode->vtotal);
    printf("  Clock: %d kHz, Flags: 0x%x\n", preferred_mode->clock / 1000, preferred_mode->flags);

    // FrameBuffer
    uint32_t db_id, pitch;
    uint64_t size;
    if (drmModeCreateDumbBuffer(fd,
        preferred_mode->hdisplay,
        preferred_mode->vdisplay,
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
    if (drmModeAddFB(fd,
        preferred_mode->hdisplay,
        preferred_mode->vdisplay,
        24,
        32,
        pitch,
        db_id, 
        &fb_id)
    ) {
        fprintf(stderr, "Failed to add framebuffer\n");
        return EXIT_FAILURE;
    }

    // Encoder
    drmModeEncoder *encoder = drmModeGetEncoder(fd, connector->encoder_id);
    if (encoder == NULL) {
        fprintf(stderr, "Failed to get drm encoder\n");
        return EXIT_FAILURE;
    }

    // CRTC
    drmModeCrtc *crtc = drmModeGetCrtc(fd, encoder->crtc_id);
    if (crtc == NULL) {
        fprintf(stderr, "Failed to get drm crtc\n");
        return EXIT_FAILURE;
    }

    // Drawing
    uint64_t db_offset;
    if (drmModeMapDumbBuffer(fd, db_id, &db_offset)) {
        fprintf(stderr, "Failed to prepare scanout buffer map\n");
        return EXIT_FAILURE;
    }

    uint32_t* db_data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, db_offset);
    memset(db_data, 0xffffffff, size);

    // Connectors are array, count is count of connectors, maybe on mirror implementation this would help.
    drmModeSetCrtc(fd, crtc->crtc_id, 0, 0, 0, NULL, 0, NULL);
    drmModeSetCrtc(fd, crtc->crtc_id, fb_id, 0, 0, &connector->connector_id, 1, preferred_mode);

    sleep(5);

    // Cleanup
    drmModeFreeConnector(connector);
    drmModeFreeResources(mode_resources);
    close(fd);

    return EXIT_SUCCESS;
}
