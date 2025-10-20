#include <pwc/render/vulkan/vk-core.h>
#include <assert.h>
#include <pwc/render/scene/scene.h>
#include <pwc/render/vulkan/vulkan.h>
#include <pwc/render/render.h>
#include <pwc/render/utils/macro.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

// Render должен запускать дисплей (Цикл, в котором проходится по всей сцене и вызывает draw)

static void init_scene(struct pwc_scene *scene) {
    SceneNodeT *root = create_scene_node(SCENE_NODE_ROOT, NULL);
    scene->root = root;

    SceneNodeT *ws1 = create_scene_node(SCENE_NODE_WORKSPACE, NULL);
    scene_add_child(root, ws1);

    SceneNodeT *bg1 = create_scene_node(SCENE_NODE_BACKGROUND, NULL);
    scene_add_child(ws1, bg1);

    SceneNodeT *ws2 = create_scene_node(SCENE_NODE_WORKSPACE, NULL);
    scene_add_child(root, ws2);

    SceneNodeT *bg2 = create_scene_node(SCENE_NODE_BACKGROUND, NULL);
    scene_add_child(ws2, bg2);

    print_scene(scene);
}

struct pwc_render *create_render(void) {
    struct pwc_render *render = calloc(1, sizeof(struct pwc_render));
    if (!render) {
        fprintf(stderr, "Failed to allocate render\n");
        return NULL;
    }

    struct pwc_vulkan *vulkan = calloc(1, sizeof(struct pwc_vulkan));
    if (!vulkan) {
        fprintf(stderr, "Failed to allocate vulkan\n");
        return NULL;
    }
    vulkan->swapchain_ready = false;
    vulkan->initialized = false;
    vulkan->validate = true;

    struct pwc_scene *scene = create_scene();
    if (!scene) {
        fprintf(stderr, "Failed to create scene\n");
        return NULL;
    }

    init_scene(scene);
    init_vulkan(vulkan);

    render->scene = scene;
    render->vulkan = vulkan;

    return render;
}

void render_destroy(struct pwc_render *render) {
    destroy_scene(render->scene);
    free(render->scene);
    render->scene = NULL;
    cleanup_vulkan(render->vulkan);
    free(render->vulkan);
    render->vulkan = NULL;

    free(render);
}

// Draw a single node (customize per type; assumes NodeRenderData in node->data)
void draw_node(SceneNodeT *node, VkCommandBuffer cmd_buffer, struct pwc_render *render, uint32_t image_index) {
    if (!node || !node->is_dirty) return;
    struct pwc_scene *scene = render->scene;
    struct pwc_vulkan *vulkan = render->vulkan;

    if (node->type == SCENE_NODE_BACKGROUND) {
        // Determine color based on parent workspace
        float color[4] = {0, 0, 0, 1};  // Default black
        if (node->parent && node->parent->type == SCENE_NODE_WORKSPACE) {
            SceneNodeT *root = scene->root;  // Assume scene is accessible; add to params if needed
            if (root->child[0] == node->parent) {
                color[0] = 1.0f;  // Red
            } else if (root->child[1] == node->parent) {
                color[2] = 1.0f;  // Blue
            }
        }

        // Begin render pass
        VkClearValue clear = {{{0, 0, 0, 1}}};
        VkRenderPassBeginInfo rp_bi = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vulkan->render_pass,
            .framebuffer = vulkan->framebuffers[image_index],  // Use the framebuffer for this image
            .renderArea = {{0, 0}, vulkan->swapchainExtent},
            .clearValueCount = 1,
            .pClearValues = &clear,
        };
        vkCmdBeginRenderPass(cmd_buffer, &rp_bi, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline and push color
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->pipeline);
        vkCmdPushConstants(cmd_buffer, vulkan->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(color), color);

        // Bind vertex buffer and draw
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vulkan->vertex_buffer, &offset);
        vkCmdDraw(cmd_buffer, 4, 1, 0, 0);

        vkCmdEndRenderPass(cmd_buffer);
    }

    node->is_dirty = false;
}

static void draw_scene_tree(SceneNodeT *node, VkCommandBuffer cmd_buffer, struct pwc_render *render, uint32_t image_index) {
    if (!node) return;
    draw_node(node, cmd_buffer, render, image_index);
    // Draw children
    for (int i = 0; i < node->num_child; i++) {
        draw_scene_tree(node->child[i], cmd_buffer, render, image_index);
    }
}

static void render_frame(struct pwc_render *render) {
    struct pwc_vulkan *vulkan = render->vulkan;
    struct pwc_scene *scene = render->scene;

    // Skip if not ready
    if (!vulkan->initialized || !vulkan->swapchain_ready) {
        return;
    }

    VkResult U_ASSERT_ONLY err;
    SubmissionResourcesT current_submission = vulkan->submission_resources[vulkan->current_submission_index];

    // Wait for fence
    // Invalid vkFence object
    vkWaitForFences(vulkan->device, 1, &current_submission.fence, VK_TRUE, UINT64_MAX);
    uint32_t current_swapchain_image_index;
    do {
        err = vkAcquireNextImageKHR(vulkan->device, vulkan->swapchain, UINT64_MAX, current_submission.image_acquired_semaphore, VK_NULL_HANDLE, &current_swapchain_image_index);
        assert(!err);
        if (!vulkan->swapchain_ready) return;
    } while(err != VK_SUCCESS);

    // Begin command buffer
    VkCommandBufferBeginInfo cmd_buf_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(current_submission.cmd, &cmd_buf_info);

    // Traverse and draw the scene
    draw_scene_tree(render->scene->root, current_submission.cmd, render, current_swapchain_image_index);
    
    vkEndCommandBuffer(current_submission.cmd);

    // Submit
    vkResetFences(vulkan->device, 1, &current_submission.fence);
    VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current_submission.image_acquired_semaphore,
        .pWaitDstStageMask = &pipe_stage_flags,
        .commandBufferCount = 1,
        .pCommandBuffers = &current_submission.cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &vulkan->draw_complete_semaphores[current_swapchain_image_index],
    };
    err = vkQueueSubmit(vulkan->graphics_queue, 1, &submit_info, current_submission.fence);
    assert(!err);

    // Present
    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vulkan->draw_complete_semaphores[current_swapchain_image_index],
        .swapchainCount = 1,
        .pSwapchains = &vulkan->swapchain,
        .pImageIndices = &current_swapchain_image_index,
    };
    err = vkQueuePresentKHR(vulkan->present_queue, &present);
    vulkan->current_submission_index = (vulkan->current_submission_index + 1) % FRAME_LAG;
    // Handle surface loss
    if (err == VK_ERROR_SURFACE_LOST_KHR) {
        vkDestroySurfaceKHR(vulkan->instance, vulkan->surface, NULL);
        create_display_surface(vulkan);  // Recreate surface (from your code)
        // Optionally recreate swapchain here if needed
    }
}

void render_run(struct pwc_render *render) {
    render->running = true;
    while (render->running) {
        render_frame(render);

        // Check for quit
    }

    render_destroy(render);
}