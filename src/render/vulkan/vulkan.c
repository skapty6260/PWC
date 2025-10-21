#include <pwc/render/vulkan/demo.h>
#include <pwc/render/vulkan/vulkan.h>
#include <pwc/render/vulkan/vk-core.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

void cleanup_vulkan(struct pwc_vulkan *vulkan) {
    if (vulkan->framebuffers) {
        for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) vkDestroyFramebuffer(vulkan->device, vulkan->framebuffers[i], NULL);
        free(vulkan->framebuffers);
    }
    if (vulkan->pipeline) vkDestroyPipeline(vulkan->device, vulkan->pipeline, NULL);
    if (vulkan->pipeline_layout) vkDestroyPipelineLayout(vulkan->device, vulkan->pipeline_layout, NULL);
    if (vulkan->render_pass) vkDestroyRenderPass(vulkan->device, vulkan->render_pass, NULL);
    if (vulkan->vertex_buffer) vkDestroyBuffer(vulkan->device, vulkan->vertex_buffer, NULL);
    if (vulkan->vertex_mem) vkFreeMemory(vulkan->device, vulkan->vertex_mem, NULL);
    if (vulkan->vert_shader) vkDestroyShaderModule(vulkan->device, vulkan->vert_shader, NULL);
    if (vulkan->frag_shader) vkDestroyShaderModule(vulkan->device, vulkan->frag_shader, NULL);
    if (vulkan->fences) {
        for (int i = 0; i < FRAME_LAG; i++) vkDestroyFence(vulkan->device, vulkan->fences[i], NULL);
        free(vulkan->fences);
    }
    if (vulkan->draw_complete_semaphores) {
        for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) vkDestroySemaphore(vulkan->device, vulkan->draw_complete_semaphores[i], NULL);
        free(vulkan->draw_complete_semaphores);
    }
    if (vulkan->image_acquired_semaphores) {
        for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) vkDestroySemaphore(vulkan->device, vulkan->image_acquired_semaphores[i], NULL);
        free(vulkan->image_acquired_semaphores);
    }
    if (vulkan->swapchain_image_views) {
        for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) vkDestroyImageView(vulkan->device, vulkan->swapchain_image_views[i], NULL);
        free(vulkan->swapchain_image_views);
    }
    if (vulkan->swapchain) vkDestroySwapchainKHR(vulkan->device, vulkan->swapchain, NULL);
    free(vulkan->swapchain_images);
    if (vulkan->cmd_pool) vkDestroyCommandPool(vulkan->device, vulkan->cmd_pool, NULL);  // Added
    if (vulkan->device) vkDestroyDevice(vulkan->device, NULL);
    if (vulkan->instance) vkDestroyInstance(vulkan->instance, NULL);
}

int init_vulkan(struct pwc_vulkan *vulkan) {
    create_vulkan_instance(vulkan);
    pick_physical_device(vulkan);
    create_display_surface(vulkan);
    create_swapchain(vulkan);
    create_image_views(vulkan);
    create_graphics_pipeline(vulkan);

    return EXIT_SUCCESS;
};