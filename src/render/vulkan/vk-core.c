#include <assert.h>
#include <dlfcn.h>
#include <pwc/render/vulkan.h>
#include <pwc/render/vulkan/vk-core.h>
#include <pwc/render/vulkan/vk-debug.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

#if defined(NDEBUG) && defined(__GNUC__)
#define U_ASSERT_ONLY __attribute__((unused))
#else
#define U_ASSERT_ONLY
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

const char *device_extensions[1] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
const uint32_t device_extensions_count = 1;

static bool check_device_extensions_support(VkPhysicalDevice device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    VkExtensionProperties available_extensions[extension_count];
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    for (uint32_t i = 0; i < ARRAY_SIZE(device_extensions); i++) {
        bool ext_found = false;
        for (uint32_t j = 0; j < extension_count; j++) {
            if (!strcmp(device_extensions[i], available_extensions[j].extensionName)) {
                printf("Device Extension active %s\n", available_extensions[j].extensionName);
                ext_found = true;   
                break;
            }
        }
        if (!ext_found) {
            return false;
        }
    }

    return true;
}

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

    // Check device support required queue families
    QueueFamilyIndices indices = find_queue_families(physical_device);
    if (!indices.is_set) {
        fprintf(stderr, "GPU does not support required queue families\n");
        score = 0;
    }

    // Check extensions support
    if (!check_device_extensions_support(physical_device)) {
        fprintf(stderr, "GPU doesn't support required extensions\n");
        score = 0;
    }

    return score;
}

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
    assert(!err);

    display = display_props.display;

    // INSIDE BUSY GPU ERRORS HERE
    // Get the first mode of the display
    if (vkGetDisplayModePropertiesKHR(vulkan->physicalDevice, display, &mode_count, NULL) != VK_SUCCESS && mode_count == 0) {
        fprintf(stderr, "Cannot find any mode for the display\n");
        exit(EXIT_FAILURE);
    }

    // TTY ERRORS HERE
    mode_count = 1;
    err = vkGetDisplayModePropertiesKHR(vulkan->physicalDevice, display, &mode_count, &mode_props);
    assert(!err);

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


    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queueCreateInfo,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = &features,
        .enabledExtensionCount = device_extensions_count,
        .ppEnabledExtensionNames = device_extensions,
    };

    if (vkCreateDevice(vulkan->physicalDevice, &createInfo, NULL, &vulkan->device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device\n");
        exit(EXIT_FAILURE);
    }

    vkGetDeviceQueue(vulkan->device, indices.graphics_family, 0, &vulkan->graphics_queue);
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
