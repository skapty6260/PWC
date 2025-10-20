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

#define FRAME_LAG 2

struct pwc_demo;

typedef struct SubmissionResources {
    VkCommandBuffer cmd;
    VkFence fence;
    VkSemaphore image_acquired_semaphore;
} SubmissionResourcesT;

struct pwc_vulkan {
    VkSurfaceKHR surface;
    // struct pwc_demo *demo;
    VkPhysicalDeviceMemoryProperties memory_properties;

    VkInstance instance;
    VkDevice device;
    VkDisplayKHR display;
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
    SubmissionResourcesT submission_resources[FRAME_LAG];
    uint32_t current_submission_index;

    uint32_t swapchain_image_count;
    VkImage *swapchain_images;  // Array of swapchain images
    VkImageView *swapchain_image_views;  // Array of image views
    VkSemaphore *image_acquired_semaphores;  // Per image
    VkSemaphore *draw_complete_semaphores;   // Per image
    VkFence *fences;  // Per submission (FRAME_LAG)
    
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd;

    VkCommandPool present_cmd_pool;

    VkRenderPass render_pass;  // For rendering to swapchain
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;  // Shared for backgrounds
    VkBuffer vertex_buffer;  // Shared quad buffer
    VkDeviceMemory vertex_mem;
    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
    VkExtent2D swapchainExtent;  // Added: Store the swapchain's extent
    // Framebuffers for render pass (one per swapchain image)
    VkFramebuffer *framebuffers;

    uint32_t enabled_extension_count;
    uint32_t enabled_layer_count;
    char *extension_names[64];
    char *enabled_layers[64];

    // VkFormat format;
    // VkColorSpaceKHR color_space;

    bool validate;
    bool initialized;
    bool swapchain_ready;
};

int init_vulkan(struct pwc_vulkan *vulkan);
void cleanup_vulkan(struct pwc_vulkan *vulkan);

#endif