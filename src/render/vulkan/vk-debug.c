#include <pwc/render/vulkan/vk-debug.h>
#include <vulkan/vulkan.h>

#include <stdio.h>
#include <string.h>

const char* validation_layers = {
    "VK_LAYER_KHRONOS_validation",
};
const uint32_t validation_layer_count = 1;

#ifdef NDEBUG
const bool enable_validation_layers = false;
#else
const bool enable_validation_layers = true;
#endif

bool check_layer_validation_support(void) {
    uint32_t layers_count;
    vkEnumerateInstanceLayerProperties(&layers_count, NULL);

    VkLayerProperties available_layers[layers_count];
    vkEnumerateInstanceLayerProperties(&layers_count, available_layers);

    for (uint32_t i = 0; i < validation_layer_count; i++) {
        bool layer_found = false;
        for (uint32_t j = 0; j < layers_count; j++) {
            if (strcmp(available_layers[j].layerName, &validation_layers[i]) == 0) {
                layer_found = true;  
                printf("Layer %s\n", available_layers[j].layerName);
                break;
            }
        }
        if (!layer_found) {
            return false;
        }
    }

    return true;
}