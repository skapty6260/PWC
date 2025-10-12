#include <pwc/vulkan.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define APP_NAME "pwc vulkan wayland compositor"

static uint32_t rate_device_suitability(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    uint32_t score = 0;

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    score += properties.limits.maxImageDimension2D;

    // Can't work without geometry shader
    if (!features.geometryShader) {
        score = 0;
    }

    // Check device support required queue families
    QueueFamilyIndices indices = find_queue_families(physical_device);
    if (!indices.is_set) {
        fprintf(stderr, "GPU does not support required queue families\n");
        score = 0;
    }

    return score;
}

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

    vulkan->physicalDevice = device;

    // Log info
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    printf(
        "Selected device %s:\nDevice Id: %u\nDevice Type: %u\nVendor Id: %u\nDriver Version: %u\n",
        properties.deviceName, properties.deviceID, properties.deviceType, properties.vendorID, properties.driverVersion
    );
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

static void create_logical_device(struct pwc_vulkan *vulkan) {
    QueueFamilyIndices indices = find_queue_families(vulkan->physicalDevice);
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(vulkan->physicalDevice, &features);

    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = indices.graphics_family,
        .queueCount = 1
    };

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queueCreateInfo,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = &features,
        .enabledExtensionCount = 0
    };

    if (vkCreateDevice(vulkan->physicalDevice, &createInfo, NULL, &vulkan->device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device\n");
        exit(EXIT_FAILURE);
    }

    vkGetDeviceQueue(vulkan->device, indices.graphics_family, 0, &vulkan->graphics_queue);
}

static void create_render_pass(struct pwc_vulkan *vulkan) {
    VkAttachmentDescription colorAttachment = {
        .format = vulkan->image_format,
        .samples = 1,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentsRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference resolveAttachmentsRef = {
        .attachment = VK_ATTACHMENT_UNUSED,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpasses = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentsRef,
        .pResolveAttachments = &resolveAttachmentsRef,
        .pDepthStencilAttachment = NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .subpassCount = 1,
        .pAttachments = &colorAttachment,
        .pSubpasses = &subpasses,
        .dependencyCount = 0,
    };

    if (vkCreateRenderPass(vulkan->device, &renderPassCreateInfo, NULL, &vulkan->render_pass) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vulkan render-pass\n");
        exit(EXIT_FAILURE);
    }
}

static void create_command_pool(struct pwc_vulkan *vulkan) {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = 0,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };

    if (vkCreateCommandPool(vulkan->device, &commandPoolCreateInfo, NULL, &vulkan->command_pool) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vulkan command pool\n");
        exit(EXIT_FAILURE);
    }
}

static void create_semaphore(struct pwc_vulkan *vulkan) {
    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    if (vkCreateSemaphore(vulkan->device, &semaphoreCreateInfo, NULL, &vulkan->semaphore) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vulkan semaphore\n");
        exit(EXIT_FAILURE);
    }
}

static void create_graphics_pipeline(struct pwc_vulkan *vulkan) {

}

// Swapchain нужен только для extensions (GLFW) для DRM похуй братка.

void cleanup_vulkan(struct pwc_vulkan *vulkan) {
    vkDestroyDevice(vulkan->device, NULL);
    vkDestroyInstance(vulkan->instance, NULL);
}

int init_vulkan(struct pwc_vulkan *vulkan, uint32_t drm_fd) {
    create_vulkan_instance(vulkan);
    pick_physical_device(vulkan);
    create_logical_device(vulkan);
    create_render_pass(vulkan);
    create_command_pool(vulkan);
    create_semaphore(vulkan);

    return EXIT_SUCCESS;
};