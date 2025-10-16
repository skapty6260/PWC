#ifndef _PWC_RENDER_DRM_H
#define _PWC_RENDER_DRM_H

#include <pwc/render/vulkan.h>
#include <xf86drmMode.h>
#include <gbm.h>

struct pwc_drm {
    int fd;

    drmModeResPtr mode_resources;
    drmModeConnectorPtr connector;
    drmModeModeInfoPtr preferred_mode;

    drmModeEncoder *encoder;
    drmModeCrtc *crtc;
};

drmModeConnectorPtr getFirstConnectedConnector(drmModeResPtr mode_resources, int fd);
drmModeModeInfoPtr getPreferredMode(drmModeConnectorPtr connector, int fd);

int init_drm(struct pwc_drm *drm, char path[32]);

#endif