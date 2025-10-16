#ifndef _PWC_RENDER_VULKAN_CORE
#define _PWC_RENDER_VULKAN_CORE

#include <stdbool.h>
#include <stdint.h>

struct pwc_vulkan;

void create_vulkan_instance(struct pwc_vulkan *vulkan);

void create_render_pass(struct pwc_vulkan *vulkan);
void create_command_pool(struct pwc_vulkan *vulkan);
void create_semaphore(struct pwc_vulkan *vulkan);
void create_logical_device(struct pwc_vulkan *vulkan);
void pick_physical_device(struct pwc_vulkan *vulkan);
void create_display_surface(struct pwc_vulkan *vulkan);

#endif