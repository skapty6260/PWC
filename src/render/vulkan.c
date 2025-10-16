#include <pwc/render/vulkan.h>
#include <pwc/render/vulkan/vk-core.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

QueueFamilyIndices find_queue_families(VkPhysicalDevice physical_device) {
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, NULL);
    if (count == 0) {
        fprintf(stderr, "failed to get device queue family properties\n");
        exit(EXIT_FAILURE);
    }

    VkQueueFamilyProperties props[count];
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, props);
    
    for (int i = 0; i < count; i++) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.is_set = true;
            break;
        }
    }

    return indices;
}

void cleanup_vulkan(struct pwc_vulkan *vulkan) {
    vkDestroyDevice(vulkan->device, NULL);
    vkDestroyInstance(vulkan->instance, NULL);
}

int init_vulkan(struct pwc_vulkan *vulkan) {
    create_vulkan_instance(vulkan);
    pick_physical_device(vulkan);
    create_logical_device(vulkan);
    printf("Created vulkan success");
    create_display_surface(vulkan);
    // init_vk_swapchain
    // demo_prepare
    // demo_run_display

    // vulkan_cleanup
    // create_render_pass(vulkan);
    // create_command_pool(vulkan);
    // create_semaphore(vulkan);

    return EXIT_SUCCESS;
};