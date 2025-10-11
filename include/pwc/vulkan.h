#ifndef _PWC_VULKAN_H
#define _PWC_VULKAN_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct pwc_vulkan {
    VkInstance instance;
    VkPhysicalDevice device;
    VkDisplayKHR display;
};

typedef struct QueueFamilyIndices {
    uint32_t graphics_family;
    bool is_set;
} QueueFamilyIndices;

int init_vulkan(struct pwc_vulkan *vulkan, uint32_t drm_fd);
QueueFamilyIndices find_queue_families(VkPhysicalDevice device);

#endif