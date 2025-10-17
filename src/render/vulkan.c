#include <pwc/render/demo.h>
#include <pwc/render/vulkan.h>
#include <pwc/render/vulkan/vk-core.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cleanup_vulkan(struct pwc_vulkan *vulkan) {
    vkDestroyDevice(vulkan->device, NULL);
    vkDestroyInstance(vulkan->instance, NULL);
}

int init_vulkan(struct pwc_vulkan *vulkan) {
    create_vulkan_instance(vulkan);
    pick_physical_device(vulkan);
    create_display_surface(vulkan);
    init_swapchain(vulkan);

    printf("Created vulkan success\nInitialized swapchain (So should be logical device and surface)\n\nRunning demo...\n");
    demo_prepare(vulkan);
    demo_run_display(vulkan);

    cleanup_vulkan(vulkan);

    return EXIT_SUCCESS;
};