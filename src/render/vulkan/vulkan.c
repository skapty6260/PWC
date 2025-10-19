#include <pwc/render/vulkan/demo.h>
#include <pwc/render/vulkan/vulkan.h>
#include <pwc/render/vulkan/vk-core.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

void cleanup_vulkan(struct pwc_vulkan *vulkan) {
    vkDestroyDevice(vulkan->device, NULL);
    vkDestroyInstance(vulkan->instance, NULL);
}

// TODO: Remove all demo functions and start working on scene.
// Scene should be independent of Vulkan, and only be passed to render, that will use some vulkan methods to draw/redraw/erase, etc.

int init_vulkan(struct pwc_vulkan *vulkan) {
    create_vulkan_instance(vulkan);
    demo_init(vulkan);
    pick_physical_device(vulkan);
    create_display_surface(vulkan);
    init_swapchain(vulkan);
    demo_prepare(vulkan);
    demo_run_display(vulkan->demo);
    cleanup_vulkan(vulkan);

    return EXIT_SUCCESS;
};