#ifndef _PWC_VULKAN_H
#define _PWC_VULKAN_H

#include <gbm.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct pwc_vulkan {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkDisplayKHR display;
    VkQueue graphics_queue;
    VkRenderPass render_pass;
    VkCommandPool command_pool;
    VkSemaphore semaphore;

    // FrameBuffer (gbm)
    struct gbm_device *gbm_device;
    struct gbm_bo *gbm_bo;
    uint32_t fb;

    VkFormat image_format;
};

typedef struct QueueFamilyIndices {
    uint32_t graphics_family;
    bool is_set;
} QueueFamilyIndices;

int init_vulkan(struct pwc_vulkan *vulkan, uint32_t drm_fd);
QueueFamilyIndices find_queue_families(VkPhysicalDevice device);

#endif