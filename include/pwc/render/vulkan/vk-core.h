#ifndef _PWC_RENDER_VULKAN_CORE
#define _PWC_RENDER_VULKAN_CORE

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

struct pwc_vulkan;

typedef struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t format_count;
    uint32_t present_count;
    VkSurfaceFormatKHR *formats;
    VkPresentModeKHR *present_modes;
    VkBool32 *supports_present;
} SwapChainSupportDetails;

SwapChainSupportDetails query_swap_chain_support(struct pwc_vulkan *vulkan);
VkSurfaceFormatKHR choose_swap_surface_mode(const VkSurfaceFormatKHR *surface_formats, uint32_t count);
VkPresentModeKHR choose_swap_present_mode(uint32_t modes_count, VkPresentModeKHR *available_modes);
VkExtent2D choose_swap_surface_extent(VkSurfaceCapabilitiesKHR capabilities);
VkCompositeAlphaFlagBitsKHR choose_swap_alpha_mode(VkSurfaceCapabilitiesKHR capabilities);
VkSurfaceTransformFlagsKHR choose_swap_pre_transform(VkSurfaceCapabilitiesKHR capabilities);
uint32_t get_swap_image_count(VkSurfaceCapabilitiesKHR capabilities);

void create_vulkan_instance(struct pwc_vulkan *vulkan);

void create_logical_device(struct pwc_vulkan *vulkan);
void pick_physical_device(struct pwc_vulkan *vulkan);
void create_display_surface(struct pwc_vulkan *vulkan);
void create_swapchain(struct pwc_vulkan *vulkan);
void create_image_views(struct pwc_vulkan *vulkan);
void create_graphics_pipeline(struct pwc_vulkan *vulkan);
void prepare_vulkan(struct pwc_vulkan *vulkan);

#endif