#ifndef _PWC_RENDER_VULKAN_DEBUG
#define _PWC_RENDER_VULKAN_DEBUG

#include <stdbool.h>
#include <stdint.h>

extern const bool enable_validation_layers;
extern const char* validation_layers;
extern const uint32_t validation_layer_count;

bool check_layer_validation_support(void);

#endif