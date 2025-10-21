#include <assert.h>
#include <dlfcn.h>
#include <pwc/render/vulkan/vulkan.h>
#include <pwc/render/vulkan/vk-core.h>
#include <pwc/render/vulkan/vk-debug.h>
#include <pwc/render/utils/macro.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

static bool check_device_extensions_support(struct pwc_vulkan *vulkan, VkPhysicalDevice physical_device) {
    VkResult U_ASSERT_ONLY err;

    uint32_t device_extensions_count = 0;
    VkBool32 swapchainExtFound = 0;
    vulkan->enabled_extension_count = 0;
    memset(vulkan->extension_names, 0, sizeof(vulkan->extension_names));
    
    err = vkEnumerateDeviceExtensionProperties(physical_device, NULL, &device_extensions_count, NULL);
    assert(!err);

    if (device_extensions_count > 0) {
        VkExtensionProperties *device_extensions = malloc(sizeof(VkExtensionProperties) * device_extensions_count);
        err = vkEnumerateDeviceExtensionProperties(physical_device, NULL, &device_extensions_count, device_extensions);
        assert(!err);

        for (uint32_t i = 0; i < device_extensions_count; i++) {
            if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, device_extensions[i].extensionName)) {
                swapchainExtFound = 1;
                vulkan->extension_names[vulkan->enabled_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            };
        }

        assert(vulkan->enabled_extension_count < 64);
        free(device_extensions);
    }

    return swapchainExtFound;
}

static uint32_t rate_device_suitability(VkPhysicalDevice physical_device, struct pwc_vulkan *vulkan) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    uint32_t score = 0;

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    score += properties.limits.maxImageDimension2D;

    // Check extensions support
    if (!check_device_extensions_support(vulkan, physical_device)) {
        fprintf(stderr, "GPU doesn't support required extensions\n");
        return 0;
    }

    return score;
}

void pick_physical_device(struct pwc_vulkan *vulkan) {
    VkResult err;

    // Get devices count and check if at least 1 available
    uint32_t deviceCount = 0;
    err = vkEnumeratePhysicalDevices(vulkan->instance, &deviceCount, NULL);
    assert(!err);

    if (deviceCount == 0) {
        fprintf(stderr, "failed to find GPUs with Vulkan support!");
        exit(EXIT_FAILURE);
    }

    // Get first suitable device
    VkPhysicalDevice devices[deviceCount];
    err = vkEnumeratePhysicalDevices(vulkan->instance, &deviceCount, devices);
    assert(!err);

    // Most suitable device
    VkPhysicalDevice device;
    uint32_t device_score;
    for (int i = 0; i < deviceCount; i++) {
        uint32_t score = rate_device_suitability(devices[i], vulkan);
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

    // Get properties and queueFamilies
    vkGetPhysicalDeviceProperties(vulkan->physicalDevice, &vulkan->gpu_props);
    
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan->physicalDevice, &vulkan->queue_family_count, NULL);
    assert(vulkan->queue_family_count >= 1);

    vulkan->queue_props = (VkQueueFamilyProperties *)malloc(vulkan->queue_family_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan->physicalDevice, &vulkan->queue_family_count, vulkan->queue_props);

    // Query fine-grained feature for this device.
    //  if app has specific feature requirements it should check supported
    //  features based on this query
    VkPhysicalDeviceFeatures physical_device_features;
    vkGetPhysicalDeviceFeatures(vulkan->physicalDevice, &physical_device_features);

    // Log info
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    printf(
        "\nSelected device %s\n\n",
        properties.deviceName
    );
}

void create_logical_device(struct pwc_vulkan *vulkan) {
    VkResult U_ASSERT_ONLY err;
    float queue_priorities[1] = {0.0};
    VkDeviceQueueCreateInfo queues[2];
    queues[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queues[0].pNext = NULL;
    queues[0].queueFamilyIndex = vulkan->graphics_queue_family_index;
    queues[0].queueCount = 1;
    queues[0].pQueuePriorities = queue_priorities;
    queues[0].flags = 0;

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(vulkan->physicalDevice, &features);

    VkDeviceCreateInfo device = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = queues,
        .enabledExtensionCount = vulkan->enabled_extension_count, 
        .ppEnabledExtensionNames = (const char *const *)vulkan->extension_names, // (const char *const *)vulkan->extension_names,
        .pEnabledFeatures = &features,
    };

    // Present queue
    if (vulkan->separate_present_queue) {
        queues[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queues[1].pNext = NULL;
        queues[1].queueFamilyIndex = vulkan->present_queue_family_index;
        queues[1].queueCount = 1;
        queues[1].pQueuePriorities = queue_priorities;
        queues[1].flags = 0;
        device.queueCreateInfoCount = 2;
    }

    err = vkCreateDevice(vulkan->physicalDevice, &device, NULL, &vulkan->device);
    assert(!err);
}

// ==============================================================================================
//                                             SURFACE
// ==============================================================================================

void create_display_surface(struct pwc_vulkan *vulkan) {
    VkResult U_ASSERT_ONLY err;
    uint32_t display_count;
    uint32_t mode_count;
    uint32_t plane_count;
    VkDisplayPropertiesKHR display_props;
    VkDisplayKHR display;
    VkDisplayModePropertiesKHR mode_props;
    VkDisplayPlanePropertiesKHR *plane_props;
    VkBool32 found_plane = VK_FALSE;
    uint32_t plane_index;
    VkExtent2D image_extent;
    VkDisplaySurfaceCreateInfoKHR create_info;

    // Get first display
    display_count = 1;
    err = vkGetPhysicalDeviceDisplayPropertiesKHR(vulkan->physicalDevice, &display_count, &display_props);
    assert(!err || (err == VK_INCOMPLETE));

    display = display_props.display;

    // Get the first mode of the display
    err = vkGetDisplayModePropertiesKHR(vulkan->physicalDevice, display, &mode_count, NULL);
    assert(!err);
        
    if (mode_count == 0) {
    	fprintf(stderr, "Cannot find any mode for the display\n");
	    exit(EXIT_FAILURE);
    }

    mode_count = 1;
    err = vkGetDisplayModePropertiesKHR(vulkan->physicalDevice, display, &mode_count, &mode_props);
    assert(!err || (err == VK_INCOMPLETE));

    // Get the list of planes
    if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vulkan->physicalDevice, &plane_count, NULL) != VK_SUCCESS && plane_count == 0) {
        fprintf(stderr, "Cannot find any plane\n");
        exit(EXIT_FAILURE);
    }

    plane_props = malloc(sizeof(VkDisplayPlanePropertiesKHR) * plane_count);
    assert(plane_props);

    err = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vulkan->physicalDevice, &plane_count, plane_props);
    assert(!err);

    // Find a plane compatible with the display
    for (plane_index = 0; plane_index < plane_count; plane_index++) {
        uint32_t supported_count;
        VkDisplayKHR *supported_displays;

        // Skip planes that are bound to a different display
        if ((plane_props[plane_index].currentDisplay != VK_NULL_HANDLE) && (plane_props[plane_index].currentDisplay != display)) {
            continue;
        }

        err = vkGetDisplayPlaneSupportedDisplaysKHR(vulkan->physicalDevice, plane_index, &supported_count, NULL);
        assert(!err);

        if (supported_count == 0) {
            continue;
        }

        supported_displays = malloc(sizeof(VkDisplayKHR) * supported_count);
        assert(supported_displays);

        err = vkGetDisplayPlaneSupportedDisplaysKHR(vulkan->physicalDevice, plane_index, &supported_count, supported_displays);
        assert(!err);

        for (uint32_t i = 0; i < supported_count; i++) {
            if (supported_displays[i] == display) {
                found_plane = VK_TRUE;
                break;
            }
        }

        free(supported_displays);

        if (found_plane) {
            break;
        }
    }

    if (!found_plane) {
        fprintf(stderr, "Failed to find a plane compatible with the display\n");
        exit(EXIT_FAILURE);
    }

    VkDisplayPlaneCapabilitiesKHR plane_capabilities;
    vkGetDisplayPlaneCapabilitiesKHR(vulkan->physicalDevice, mode_props.displayMode, plane_index, &plane_capabilities);
    // Find supported alpha mode
    VkDisplayPlaneAlphaFlagBitsKHR alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    VkDisplayPlaneAlphaFlagBitsKHR alphaModes[4] = {
        VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR,
        VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR,
        VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR,
        VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR,
    };
    for (uint32_t i = 0; i < sizeof(alphaModes); i++) {
        if (plane_capabilities.supportedAlpha & alphaModes[i]) {
            alphaMode = alphaModes[i];
            break;
        }
    }
    image_extent.width = mode_props.parameters.visibleRegion.width;
    image_extent.height = mode_props.parameters.visibleRegion.height;

    create_info.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.displayMode = mode_props.displayMode;
    create_info.planeIndex = plane_index;
    create_info.planeStackIndex = plane_props[plane_index].currentStackIndex;
    create_info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create_info.alphaMode = alphaMode;
    create_info.globalAlpha = 1.0f;
    create_info.imageExtent = image_extent;

    free(plane_props);

    err = vkCreateDisplayPlaneSurfaceKHR(vulkan->instance, &create_info, NULL, &vulkan->surface);
    assert(!err);
}

// ==============================================================================================
//                                            SWAPCHAIN
// ==============================================================================================

SwapChainSupportDetails query_swap_chain_support(struct pwc_vulkan *vulkan) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan->physicalDevice, vulkan->surface, &details.capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physicalDevice, vulkan->surface, &format_count, NULL);
    details.format_count = format_count;
    VkSurfaceFormatKHR formats[format_count];
    details.formats = formats;
    if (format_count != 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physicalDevice, vulkan->surface, &format_count, details.formats);
    }

    uint32_t present_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physicalDevice, vulkan->surface, &present_count, NULL);
    details.present_count = present_count;
    VkPresentModeKHR present_modes[present_count];
    details.present_modes = present_modes;
    if (present_count != 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physicalDevice, vulkan->surface, &present_count, details.present_modes);
    }

    // Check if supports present
    VkBool32 *supports_present = (VkBool32 *)malloc(vulkan->queue_family_count * sizeof(VkBool32));
    details.supports_present = supports_present;
    for (uint32_t i = 0; i < vulkan->queue_family_count; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(vulkan->physicalDevice, i, vulkan->surface, &supports_present[i]);
    }

    return details;
}

VkSurfaceFormatKHR choose_swap_surface_mode(const VkSurfaceFormatKHR *surface_formats, uint32_t count) {
    // Prefer non-SRGB formats
    for (uint32_t i = 0; i < count; i++) {
        const VkFormat format = surface_formats[i].format;

         if (format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_B8G8R8A8_UNORM ||
            format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
            format == VK_FORMAT_A1R5G5B5_UNORM_PACK16 || format == VK_FORMAT_R5G6B5_UNORM_PACK16 ||
            format == VK_FORMAT_R16G16B16A16_SFLOAT) {
            return surface_formats[i];
        }
    }

    printf("Can't find our preferred formats... Falling back to first exposed format. Rendering may be incorrect.\n");

    // assert(count >= 1);
    return surface_formats[0];
}

VkPresentModeKHR choose_swap_present_mode(uint32_t modes_count, VkPresentModeKHR *available_modes) {
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swap_surface_extent(VkSurfaceCapabilitiesKHR capabilities) {
    VkExtent2D swapchainExtent;
    // width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
    if (capabilities.currentExtent.width == 0xFFFFFFFF) {
        // If the surface size is undefined, the size is set to the size
        // of the images requested, which must fit within the minimum and
        // maximum values.
        // swapchainExtent.width = demo->width;
        // swapchainExtent.height = demo->height;

        if (swapchainExtent.width < capabilities.minImageExtent.width) {
            swapchainExtent.width = capabilities.minImageExtent.width;
        } else if (swapchainExtent.width > capabilities.maxImageExtent.width) {
            swapchainExtent.width = capabilities.maxImageExtent.width;
        }

        if (swapchainExtent.height < capabilities.minImageExtent.height) {
            swapchainExtent.height = capabilities.minImageExtent.height;
        } else if (swapchainExtent.height > capabilities.maxImageExtent.height) {
            swapchainExtent.height = capabilities.maxImageExtent.height;
        }
    } else {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = capabilities.currentExtent;
        // demo->width = surfCapabilities.currentExtent.width;
        // demo->height = surfCapabilities.currentExtent.height;
    }

    return swapchainExtent;
}

VkCompositeAlphaFlagBitsKHR choose_swap_alpha_mode(VkSurfaceCapabilitiesKHR capabilities) {
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[4] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (uint32_t i = 0; i < ARRAY_SIZE(compositeAlphaFlags); i++) {
        if (capabilities.supportedCompositeAlpha & compositeAlphaFlags[i]) {
            compositeAlpha = compositeAlphaFlags[i];
            break;
        }
    }

    return compositeAlpha;
}

VkSurfaceTransformFlagsKHR choose_swap_pre_transform(VkSurfaceCapabilitiesKHR capabilities) {
    VkSurfaceTransformFlagsKHR pre_transform;
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        pre_transform = capabilities.currentTransform;
    }

    return pre_transform;
}

uint32_t get_swap_image_count(VkSurfaceCapabilitiesKHR capabilities) {
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    return image_count;
}

QueueFamilyData get_queue_family_data(struct pwc_vulkan *vulkan, VkBool32 *supports_present) {
    QueueFamilyData data;

    uint32_t graphics_queue_family_index = UINT32_MAX;
    uint32_t present_queue_family_index = UINT32_MAX;
    for (uint32_t i = 0; i < vulkan->queue_family_count; i++) {
            if ((vulkan->queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                if (graphics_queue_family_index == UINT32_MAX) {
                    graphics_queue_family_index = i;
                }

                if (supports_present[i] == VK_TRUE) {
                    graphics_queue_family_index = i;
                    present_queue_family_index = i;
                    break;
                }
            }
    }

    if (present_queue_family_index == UINT32_MAX) {
        // If didn't find a queue that supports both graphics and present, then
        // find a separate present queue.
        for (uint32_t i = 0; i < vulkan->queue_family_count; ++i) {
            present_queue_family_index = i;
            break;
        }
    }

    // Generate error if couldn't find both graphics and present queue
    if (graphics_queue_family_index == UINT32_MAX || present_queue_family_index == UINT32_MAX) {
        fprintf(stderr, "[Swapchain initialization failure]\nCouldn't find both graphics and present queues\n");
        exit(EXIT_FAILURE);
    }

    data.graphics_queue_family_index = graphics_queue_family_index;
    data.present_queue_family_index = present_queue_family_index;
    data.separate_present_queue = (graphics_queue_family_index != present_queue_family_index);

    return data;
}

void create_swapchain(struct pwc_vulkan *vulkan) {
    VkResult U_ASSERT_ONLY err;
    SwapChainSupportDetails swapchain_details = query_swap_chain_support(vulkan);
    
    VkSurfaceFormatKHR surface_format = choose_swap_surface_mode(swapchain_details.formats, swapchain_details.format_count);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_details.present_count, swapchain_details.present_modes);
    VkExtent2D extent = choose_swap_surface_extent(swapchain_details.capabilities);
    VkCompositeAlphaFlagBitsKHR composite_alpha = choose_swap_alpha_mode(swapchain_details.capabilities);
    VkSurfaceTransformFlagsKHR pre_transform = choose_swap_pre_transform(swapchain_details.capabilities);
    uint32_t image_count = get_swap_image_count(swapchain_details.capabilities);

    VkSwapchainCreateInfoKHR swapchain_ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vulkan->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .presentMode = present_mode,
        .pQueueFamilyIndices = NULL,
        .queueFamilyIndexCount = 0,
        .clipped = VK_FALSE, // Can't clip when we took all display for render
        .compositeAlpha = composite_alpha,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .oldSwapchain = VK_NULL_HANDLE,
        .preTransform = pre_transform
    };

    QueueFamilyData queue_family_data = get_queue_family_data(vulkan, swapchain_details.supports_present);
    vulkan->present_queue_family_index = queue_family_data.present_queue_family_index;
    vulkan->graphics_queue_family_index = queue_family_data.graphics_queue_family_index;
    vulkan->separate_present_queue = queue_family_data.separate_present_queue;

    // if (vulkan->separate_present_queue) {
    //     swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    // } else {
    //     swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // }

    create_logical_device(vulkan);

    err = vkCreateSwapchainKHR(vulkan->device, &swapchain_ci, NULL, &vulkan->swapchain);
    assert(!err);

    vkGetSwapchainImagesKHR(vulkan->device, vulkan->swapchain, &image_count, NULL);
    vulkan->swapchain_images = (VkImage*)malloc(sizeof(VkImage) * image_count);;

    vkGetSwapchainImagesKHR(vulkan->device, vulkan->swapchain, &image_count, vulkan->swapchain_images);
    vulkan->swapchain_image_count = image_count;
    
    vulkan->swapchain_extent = extent;
    vulkan->swapchain_image_format = surface_format.format;
}

void create_image_views(struct pwc_vulkan *vulkan) {
    VkResult U_ASSERT_ONLY err;
    vulkan->swapchain_image_views = (VkImageView*)malloc(sizeof(VkImageView) * vulkan->swapchain_image_count);

    for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vulkan->swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = vulkan->swapchain_image_format,
            .components = {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
        };

        err = vkCreateImageView(vulkan->device, &create_info, NULL, &vulkan->swapchain_image_views[i]);
    }
}

// ==============================================================================================
//                                            PIPELINE
// ==============================================================================================
typedef struct ShaderFile {
  size_t size;
  char *code;
} ShaderFile;

VkShaderModule createShaderModule(struct pwc_vulkan *vulkan, ShaderFile *shader_file) {
  VkShaderModuleCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = shader_file->size,
    .pCode = (uint32_t*)shader_file->code
  };

  VkShaderModule shader_module;
  if (vkCreateShaderModule(vulkan->device, &create_info, NULL, &shader_module) != VK_SUCCESS) {
    printf("failed to create shader module!\n");
    exit(7);
  }

  return shader_module;
}

void read_file(const char *path, ShaderFile *shader) {
    FILE *pfile;

    pfile = fopen(path, "rb");
    if (pfile == NULL) {
        fprintf(stderr, "Failed to open %s\n", path);
    }

    fseek(pfile, 0L, SEEK_END);
    shader->size = ftell(pfile);

    fseek(pfile, 0L, SEEK_SET);

    shader->code = (char*)malloc(sizeof(char) * shader->size);
    size_t readCount = fread(shader->code, shader->size, sizeof(char), pfile);
    printf("ReadCount: %ld\n", readCount);

    fclose(pfile);
}

void create_graphics_pipeline(struct pwc_vulkan *vulkan) {
    ShaderFile vertShader = {0};
    ShaderFile fragShader = {0};
    read_file("/home/dietcokelover/projects/pwc/include/pwc/render/shaders/frag.spv", &fragShader);
    read_file("/home/dietcokelover/projects/pwc/include/pwc/render/shaders/vert.spv", &vertShader);
}

void init_swapchain(struct pwc_vulkan *vulkan) {
    VkResult U_ASSERT_ONLY err;

    // Iterate over each queue to learn whether it supports presenting
    VkBool32 *supports_present = (VkBool32 *)malloc(vulkan->queue_family_count * sizeof(VkBool32));
    for (uint32_t i = 0; i < vulkan->queue_family_count; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(vulkan->physicalDevice, i, vulkan->surface, &supports_present[i]);
    }

    // Seearch for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphics_queue_family_index = UINT32_MAX;
    uint32_t present_queue_family_index = UINT32_MAX;
    for (uint32_t i = 0; i < vulkan->queue_family_count; i++) {
        if ((vulkan->queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            if (graphics_queue_family_index == UINT32_MAX) {
                graphics_queue_family_index = i;
            }

            if (supports_present[i] == VK_TRUE) {
                graphics_queue_family_index = i;
                present_queue_family_index = i;
                break;
            }
        }
    }

    if (present_queue_family_index == UINT32_MAX) {
        // If didn't find a queue that supports both graphics and present, then
        // find a separate present queue.
        for (uint32_t i = 0; i < vulkan->queue_family_count; ++i) {
            present_queue_family_index = i;
            break;
        }
    }

    // Generate error if couldn't find both graphics and present queue
    if (graphics_queue_family_index == UINT32_MAX || present_queue_family_index == UINT32_MAX) {
        fprintf(stderr, "[Swapchain initialization failure]\nCouldn't find both graphics and present queues\n");
        exit(EXIT_FAILURE);
    }

    vulkan->graphics_queue_family_index = graphics_queue_family_index;
    vulkan->present_queue_family_index = present_queue_family_index;
    vulkan->separate_present_queue = (vulkan->graphics_queue_family_index != vulkan->present_queue_family_index);
    free(supports_present);

    printf("Creating logical device\n");
    create_logical_device(vulkan);

    vkGetDeviceQueue(vulkan->device, vulkan->graphics_queue_family_index, 0, &vulkan->graphics_queue);

    if (!vulkan->separate_present_queue) {
        vulkan->present_queue = vulkan->graphics_queue;
    } else {
        vkGetDeviceQueue(vulkan->device, vulkan->present_queue_family_index, 0, &vulkan->present_queue);
    }

    // Get the list of VkFormat's that are supported
    uint32_t format_count;
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physicalDevice, vulkan->surface, &format_count, NULL);
    assert(!err);
    VkSurfaceFormatKHR *surface_formats = (VkSurfaceFormatKHR *)malloc(format_count * sizeof(VkSurfaceFormatKHR));
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physicalDevice, vulkan->surface, &format_count, surface_formats);
    assert(!err);
    VkSurfaceFormatKHR surface_format = choose_swap_surface_mode(surface_formats, format_count);
    // vulkan->format = surface_format.format;
    // vulkan->color_space = surface_format.colorSpace;
    free(surface_formats);
    
    // Create command pool
    VkCommandPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vulkan->graphics_queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,  // Allow individual resets
    };
    err = vkCreateCommandPool(vulkan->device, &pool_ci, NULL, &vulkan->cmd_pool);
    assert(!err);
    // Allocate command buffers (one per FRAME_LAG)
    VkCommandBufferAllocateInfo alloc_ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vulkan->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = FRAME_LAG,
    };
    VkCommandBuffer cmd_buffers[FRAME_LAG];
    err = vkAllocateCommandBuffers(vulkan->device, &alloc_ci, cmd_buffers);
    assert(!err);
    // Assign to submission resources
    for (int i = 0; i < FRAME_LAG; i++) {
        vulkan->submission_resources[i].cmd = cmd_buffers[i];
    }

    // Get Memory information and properties
    vkGetPhysicalDeviceMemoryProperties(vulkan->physicalDevice, &vulkan->memory_properties);
}

static VkBool32 check_validation_layers(uint32_t check_count, char **check_names, uint32_t layer_count, VkLayerProperties *layers) {
    for (uint32_t i = 0; i < check_count; i++) {
        VkBool32 found = 0;
        for (uint32_t j = 0; j < layer_count; j++) {
            if (!strcmp(check_names[i], layers[j].layerName)) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Cannot find layer: %s\n", check_names[i]);
            return 0;
        }
    }
    return 1;
}

void create_vulkan_instance(struct pwc_vulkan *vulkan) {
    VkResult err;
    // Validation layers
    uint32_t instance_layer_count = 0;
    char* validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
    vulkan->enabled_layer_count = 0;

    VkBool32 validation_found = 0;
    if (vulkan->validate) {
        err = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
        assert(!err);

        if (instance_layer_count > 0) {
            VkLayerProperties *instance_layers = malloc(sizeof(VkLayerProperties) * instance_layer_count);
            err = vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layers);
            assert(!err);

            validation_found = check_validation_layers(ARRAY_SIZE(validation_layers), validation_layers, instance_layer_count, instance_layers);
        
            if (validation_found) {
                vulkan->enabled_layer_count = ARRAY_SIZE(validation_layers);
                vulkan->enabled_layers[0] = "VK_LAYER_KHRONOS_validation";
            }
            free(instance_layers);
        }

        if (!validation_found) {
            fprintf(stderr, "Failed to find required validation layers.\n\n");
            exit(EXIT_FAILURE);
        }
    }

    // Instance Extensions
    uint32_t instance_extension_count;

    VkBool32 surfaceExtFound = false;
    VkBool32 platformSurfaceExtFound = false;
    bool portabilityEnumerationActive = false;
    memset(vulkan->extension_names, 0, sizeof(vulkan->extension_names));

    err = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL);
    assert(!err);

    if (instance_extension_count > 0) {
        VkExtensionProperties *instance_extensions = malloc(sizeof(VkExtensionProperties) * instance_extension_count);
        err = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, instance_extensions);
        assert(!err);

        for (uint32_t i = 0; i < instance_extension_count; i++) {
            if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                surfaceExtFound = true;
                vulkan->extension_names[vulkan->enabled_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
            }
            if (!strcmp(VK_KHR_DISPLAY_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                platformSurfaceExtFound = true;
                vulkan->extension_names[vulkan->enabled_extension_count++] = VK_KHR_DISPLAY_EXTENSION_NAME;
            }
            if (!strcmp(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                vulkan->extension_names[vulkan->enabled_extension_count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
            }
            if (!strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                if (vulkan->validate) {
                    vulkan->extension_names[vulkan->enabled_extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
                }
            }
            // We want cube to be able to enumerate drivers that support the portability_subset extension, so we have to enable
            // the portability enumeration extension.
            if (!strcmp(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                portabilityEnumerationActive = true;
                vulkan->extension_names[vulkan->enabled_extension_count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            }
            assert(vulkan->enabled_extension_count < 64);
        }

        free(instance_extensions);
    }

    if (!surfaceExtFound) {
        fprintf(stderr, "Failed to find the " VK_KHR_SURFACE_EXTENSION_NAME);
        exit(EXIT_FAILURE);
    }
    if (!platformSurfaceExtFound) {
        fprintf(stderr, "Failed to find the " VK_KHR_DISPLAY_EXTENSION_NAME);
        exit(EXIT_FAILURE);
    }

    // Create instance
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = APP_NAME,
        .pNext = NULL,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .pNext = NULL,
        .enabledLayerCount = vulkan->enabled_layer_count,
        .ppEnabledLayerNames = (const char *const *)validation_layers,
        .enabledExtensionCount = vulkan->enabled_extension_count,
        .ppEnabledExtensionNames = (const char *const *)vulkan->extension_names,
    };

    err = vkCreateInstance(&createInfo, NULL, &vulkan->instance);
    if (err == VK_ERROR_INCOMPATIBLE_DRIVER) {
        fprintf(stderr, "Cannot find a compatible Vulkan installable client driver (ICD)\n");
        exit(EXIT_FAILURE);
    } else if (err == VK_ERROR_LAYER_NOT_PRESENT) {
        fprintf(stderr, "Cannot find a specified layer\n");
        exit(EXIT_FAILURE);
    } else if (err == VK_ERROR_EXTENSION_NOT_PRESENT) {
        fprintf(stderr, "Cannot find a specified extension library\n");
        exit(EXIT_FAILURE);
    } else if (err) {
        fprintf(stderr, "vkCreateInstanceFailed\n");
        exit(EXIT_FAILURE);
    }
}

// Prepares cmd_pool, render_pass, render_pipeline
void prepare_vulkan(struct pwc_vulkan *vulkan) {
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
        .commandBufferCount = 1,
    };
    err = vkAllocateCommandBuffers(vulkan->device, &cmd, &vulkan->cmd);
    assert(!err);

    VkCommandBufferBeginInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL,
    };
    err = vkBeginCommandBuffer(vulkan->cmd, &cmd_buf_info);
    assert(!err);

    // prepare render pass
    // prepare pipeline
}