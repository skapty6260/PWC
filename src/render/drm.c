#include <pwc/render/vulkan.h>
#include <fcntl.h>
#include <gbm.h>
#include <pwc/render/drm.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vulkan/vulkan_core.h>
#include <xf86drm.h>

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

void cleanup_drm(struct pwc_drm *drm) {
    if (drm->crtc) drmModeSetCrtc(drm->fd, drm->crtc->crtc_id, 0, 0, 0, NULL, 0, NULL);  // Disable CRTC
    drmSetMaster(drm->fd);  // Release master
    if (drm->encoder) drmModeFreeEncoder(drm->encoder);
    drmModeFreeConnector(drm->connector);
    drmModeFreeResources(drm->mode_resources);
    close(drm->fd);
    free(drm);
}

int init_drm(struct pwc_drm *drm, char path[32], struct pwc_vulkan *vulkan) {
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

    vulkan->image_format = VK_FORMAT_R8G8B8A8_SRGB;
    vulkan->width = drm->preferred_mode->hdisplay;
    vulkan->height = drm->preferred_mode->vdisplay;
    if (init_vulkan(vulkan, drm->fd)) {
        fprintf(stderr, "Failed to init vulkan");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}