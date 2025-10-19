#ifndef _PWC_RENDER_DEMO
#define _PWC_RENDER_DEMO

#include <stdbool.h>
#include <vulkan/vulkan_core.h>
#include <pwc/render/utils/linmath.h>

struct pwc_vulkan;

#define DEMO_TEXTURE_COUNT 1
#define FRAME_LAG 2
#define MAX_SWAPCHAIN_IMAGE_COUNT 8

struct texture_object {
    VkSampler sampler;

    VkImage image;
    VkBuffer buffer;
    VkImageLayout imageLayout;

    VkMemoryAllocateInfo mem_alloc;
    VkDeviceMemory mem;
    VkImageView view;
    int32_t tex_width, tex_height;
};

typedef struct {
    VkFence fence;
    VkSemaphore image_acquired_semaphore;
    VkCommandBuffer cmd;
    VkCommandBuffer graphics_to_present_cmd;
    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_memory;
    void *uniform_memory_ptr;
    VkDescriptorSet descriptor_set;
} SubmissionResources;

typedef struct {
    VkImage image;
    VkImageView view;
    VkFramebuffer framebuffer;
    VkSemaphore draw_complete_semaphore;
    VkSemaphore image_ownership_semaphore;
} SwapchainImageResources;

struct pwc_demo {
    struct pwc_vulkan *vulkan;

    SubmissionResources submission_resources[FRAME_LAG];
    uint32_t current_submission_index;

    VkShaderModule vert_shader_module;
    VkShaderModule frag_shader_module;

    VkDescriptorPool desc_pool;
    bool quit;

    struct {
        VkFormat format;

        VkImage image;
        VkMemoryAllocateInfo mem_alloc;
        VkDeviceMemory mem;
        VkImageView view;
    } demo_depth;

    mat4x4 projection_matrix;
    mat4x4 view_matrix;
    mat4x4 model_matrix;

    bool demo_quit;
    int32_t demo_current_frame;
    int32_t demo_frame_count;
    float spin_angle;
    float spin_increment;
    bool pause;

    uint32_t swapchainImageCount;
    SwapchainImageResources swapchain_resources[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkPresentModeKHR presentMode;
    bool first_swapchain_frame;
    bool swapchain_ready;

    int32_t width;
    int32_t height;

    VkPhysicalDeviceMemoryProperties memory_properties;

    struct texture_object textures[DEMO_TEXTURE_COUNT];
    struct texture_object staging_texture;

    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout desc_layout;
    VkPipelineCache pipelineCache;
    VkRenderPass render_pass;
    VkPipeline pipeline;

    bool initialized;
    bool use_staging_buffer;
};


void demo_prepare(struct pwc_vulkan *vulkan);
void demo_run_display(struct pwc_demo *demo);
void demo_init(struct pwc_vulkan *vulkan);

#endif