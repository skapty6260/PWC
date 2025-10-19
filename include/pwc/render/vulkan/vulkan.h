#ifndef _PWC_RENDER_VULKAN_H
#define _PWC_RENDER_VULKAN_H

#include <pwc/render/vulkan/demo.h>
#include <vulkan/vulkan.h>
#include <bits/types/struct_timeval.h>
#include <gbm.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define APP_NAME "pwc vulkan wayland compositor"

struct pwc_demo;

struct pwc_vulkan {
    VkSurfaceKHR surface;
    struct pwc_demo *demo;

    VkInstance instance;
    VkDevice device;
    VkDisplayKHR display;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkBuffer buffer;
    VkDeviceMemory mem;
    VkDescriptorSet descriptor_set;

    VkPhysicalDevice physicalDevice;
    VkQueue graphics_queue;
    VkQueue present_queue;
    uint32_t graphics_queue_family_index;
    uint32_t present_queue_family_index;
    uint32_t queue_family_count;
    VkPhysicalDeviceProperties gpu_props;
    VkQueueFamilyProperties *queue_props;
    bool separate_present_queue;

    VkSwapchainKHR swapchain;
    
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd;

    VkCommandPool present_cmd_pool;

    uint32_t enabled_extension_count;
    uint32_t enabled_layer_count;
    char *extension_names[64];
    char *enabled_layers[64];

    VkFormat format;
    VkColorSpaceKHR color_space;

    bool validate;
};

int init_vulkan(struct pwc_vulkan *vulkan);


#endif