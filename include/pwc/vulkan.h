#ifndef _PWC_VULKAN_H
#define _PWC_VULKAN_H

#include <limits.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct pwc_vulkan {
    VkInstance instance;
    VkPhysicalDevice device;
    VkDisplayKHR display;
};

int init_vulkan(struct pwc_vulkan *vulkan, uint32_t drm_fd);

#endif