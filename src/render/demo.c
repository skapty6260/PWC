#include <assert.h>
#include <pwc/render/demo.h>
#include <pwc/render/utils/macro.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

static void demo_name_object(struct pwc_vulkan *vulkan, VkObjectType object_type, uint64_t vulkan_handle, const char *format, ...) {
    if (!vulkan->validate) return;

    VkResult U_ASSERT_ONLY err;
    char name[1024];
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(name, sizeof(name), format, argptr);
    va_end(argptr);
    name[sizeof(name) - 1] = '\0';

    VkDebugUtilsObjectNameInfoEXT obj_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = object_type,
        .objectHandle = vulkan_handle,
        .pObjectName = name,
    };
    err = vkSetDebugUtilsObjectNameEXT(vulkan->device, &obj_name);
    assert(!err);
}

void demo_prepare(struct pwc_vulkan *vulkan) {
    VkResult U_ASSERT_ONLY err;
    if (vulkan->cmd_pool == VK_NULL_HANDLE) {
        const VkCommandPoolCreateInfo cmd_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = vulkan->graphics_queue_family_index,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        err = vkCreateCommandPool(vulkan->device, &cmd_pool_info, NULL, &vulkan->cmd_pool);
        assert(!err);
    }

    const VkCommandBufferAllocateInfo cmd = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = vulkan->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    err = vkAllocateCommandBuffers(vulkan->device, &cmd, &vulkan->cmd);
    assert(!err);
    // demo_name_object(demo, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)demo->cmd, "PrepareCB");
    VkCommandBufferBeginInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL
    };
    err = vkBeginCommandBuffer(vulkan->cmd, &cmd_buf_info);
    // demo_push_cb_label(demo, demo->cmd, NULL, "Prepare");
    assert(!err);

    // demo_prepare_textures(vulkan)
    // demo_prepare_cube_data_buffers

    // demo_prepare_descriptor_layout

    vulkan->demo_depth.format = VK_FORMAT_D16_UNORM;
    // demo_prepate_render_pass(vulkan);
    // demo_prepate_pipeline(vulkan);
}

static void demo_draw(struct pwc_vulkan *vulkan) {}

void demo_run_display(struct pwc_vulkan *vulkan) {
    while (!vulkan->demo_quit) {
        demo_draw(vulkan);
        vulkan->demo_current_frame++;

        if (vulkan->demo_frame_count != INT32_MAX && vulkan->demo_current_frame == vulkan->demo_frame_count) {
            vulkan->demo_quit = true;
        }
    }
}