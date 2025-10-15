#include <pwc/render/vulkan/vk-core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

// Все функции связанные с ядром вулкана структуры вулкана, его инициализацией, не переиспользуемыми функциями

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

    // Check extensions support
    uint32_t count;
    bool dmaBuf_supported = false;
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &count, NULL);
    VkExtensionProperties *exts = malloc(count * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &count, exts);
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(exts[i].extensionName, "VK_KHR_external_memory_fd") == 0) {
            dmaBuf_supported = true;
        }
    }
    if (!dmaBuf_supported) {
        fprintf(stderr, "GPU doesn't support dma_buffers fd\n");
        score = 0;
    }
    free(exts);

    return score;
}

void pick_physical_device(struct pwc_vulkan *vulkan) {
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

    vkGetPhysicalDeviceMemoryProperties(vulkan->physicalDevice, &vulkan->memory_properties);
    printf("Memory types available: %u\n", vulkan->memory_properties.memoryTypeCount); 
}

void create_logical_device(struct pwc_vulkan *vulkan) {
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

    const char *device_extensions[] = {
        "VK_KHR_external_memory",      // Base for external memory (core in 1.1, but enable)
        "VK_KHR_external_memory_fd",   // Import/export via POSIX FD (DMA-BUF)
        // Optional but recommended for explicit DMA-BUF:
        "VK_EXT_external_memory_dma_buf",  // Handles DMA-BUF modifiers/formats (Mesa/NVIDIA support)
        // If sync needed (e.g., between queues): "VK_KHR_external_semaphore_fd"
    };

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queueCreateInfo,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = &features,
        .enabledExtensionCount = sizeof(device_extensions) / sizeof(char*),
        .ppEnabledExtensionNames = device_extensions,
    };

    if (vkCreateDevice(vulkan->physicalDevice, &createInfo, NULL, &vulkan->device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device\n");
        exit(EXIT_FAILURE);
    }

    vkGetDeviceQueue(vulkan->device, indices.graphics_family, 0, &vulkan->graphics_queue);
}

void create_vulkan_instance(struct pwc_vulkan *vulkan) {
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

void create_render_pass(struct pwc_vulkan *vulkan) {
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

void create_command_pool(struct pwc_vulkan *vulkan) {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = 0,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };

    if (vkCreateCommandPool(vulkan->device, &commandPoolCreateInfo, NULL, &vulkan->cmd_pool) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vulkan command pool\n");
        exit(EXIT_FAILURE);
    }
};

void create_semaphore(struct pwc_vulkan *vulkan) {
    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    if (vkCreateSemaphore(vulkan->device, &semaphoreCreateInfo, NULL, &vulkan->semaphore) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vulkan semaphore\n");
        exit(EXIT_FAILURE);
    }
}

