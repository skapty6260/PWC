#ifndef _PWC_RENDER_VULKAN_H
#define _PWC_RENDER_VULKAN_H

#include <bits/types/struct_timeval.h>
#include <gbm.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define APP_NAME "pwc vulkan wayland compositor"

struct pwc_vulkan {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkDisplayKHR display;
    VkQueue graphics_queue;
    VkRenderPass render_pass;
    VkCommandPool cmd_pool;
    VkSemaphore semaphore;
    VkPipelineLayout pipeline_layout;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkPipeline pipeline;
    VkBuffer buffer;
    VkDeviceMemory mem;
    VkDescriptorSet descriptor_set;

    VkFormat image_format;
    uint32_t width, height;
};

typedef struct QueueFamilyIndices {
    uint32_t graphics_family;
    bool is_set;
} QueueFamilyIndices;

int init_vulkan(struct pwc_vulkan *vulkan, uint32_t drm_fd);
QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
int create_vulkan_image_and_export_fd(struct pwc_vulkan *vulkan, int *fd_out, VkImage *image_out);
VkResult render_red_screen_to_image(struct pwc_vulkan *vulkan, VkImage image);

#endif