#ifndef _PWC_RENDER_VULKAN_H
#define _PWC_RENDER_VULKAN_H

#include <vulkan/vulkan.h>
#include <bits/types/struct_timeval.h>
#include <gbm.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define APP_NAME "pwc vulkan wayland compositor"

struct pwc_vulkan {
    VkInstance instance;
    VkDevice device;
    VkDisplayKHR display;
    VkQueue graphics_queue;
    VkRenderPass render_pass;
    VkCommandPool cmd_pool;
    VkSemaphore semaphore;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkBuffer buffer;
    VkDeviceMemory mem;
    VkDescriptorSet descriptor_set;
    VkSwapchainKHR swapchain;

    VkSurfaceKHR surface;
    
    // GPU
    VkPhysicalDevice physicalDevice;

    uint32_t enabled_extension_count;
    uint32_t enabled_layer_count;
    char *extension_names[64];
    char *enabled_layers[64];

    bool validate;
};

typedef struct QueueFamilyIndices {
    uint32_t graphics_family;
    bool is_set;
} QueueFamilyIndices;

QueueFamilyIndices find_queue_families(VkPhysicalDevice device);

int init_vulkan(struct pwc_vulkan *vulkan);


#endif