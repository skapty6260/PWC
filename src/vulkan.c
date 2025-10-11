#include <pwc/vulkan.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define APP_NAME "pwc vulkan wayland compositor"

static uint32_t rate_device_suitability(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);

    uint32_t score = 0;

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    score += properties.limits.maxImageDimension2D;

    // Can't work without geometry shader
    if (!features.geometryShader) {
        score = 0;
    }

    return score;
}

static void pick_physical_device(struct pwc_vulkan *vulkan) {
    // Get devices count and check if at least 1 available
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vulkan->instance, &deviceCount, NULL);
    if (deviceCount == 0) {
        fprintf(stderr, "failed to find GPUs with Vulkan support!");
        exit(EXIT_FAILURE);
    }
    printf("Found %i vulkan physical devices\n", deviceCount);

    // Get first suitable device
    VkPhysicalDevice devices[deviceCount];
    vkEnumeratePhysicalDevices(vulkan->instance, &deviceCount, devices);

    VkPhysicalDevice device;
    uint32_t device_score;
    for (int i = 0; i < deviceCount; i++) {
        uint32_t score = rate_device_suitability(devices[i]);
        if (score > device_score) {
            device_score = score;
            device = devices[i];
        }
    }

    if (device == NULL) {
        fprintf(stderr, "Couldn't find suitable GPU\n");
        exit(EXIT_FAILURE);
    }

    vulkan->device = device;
}

static void create_vulkan_instance(struct pwc_vulkan *vulkan) {
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = APP_NAME,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
        .pNext = NULL
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
    };

    if (vkCreateInstance(&createInfo, NULL, &vulkan->instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vulkan instance\n");
        exit(EXIT_FAILURE);
    }
}

static void clean_vulkan_instance(struct pwc_vulkan *vulkan) {
    vkDestroyInstance(vulkan->instance, NULL);
}

int init_vulkan(struct pwc_vulkan *vulkan, uint32_t drm_fd) {
    create_vulkan_instance(vulkan);
    pick_physical_device(vulkan);

    // if (vkAcquireDrmDisplayEXT(
    //     vulkan->device,
    //     drm_fd,
    //     vulkan->display
    // ) != VK_SUCCESS) {
    //     fprintf(stderr, "Failed to acquire drm display");
    //     return EXIT_FAILURE;
    // }

    return EXIT_SUCCESS;
};