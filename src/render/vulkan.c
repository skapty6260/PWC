#include <pwc/render/vulkan.h>
#include <pwc/render/vulkan/vk-core.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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

static uint32_t find_memory_type(struct pwc_vulkan *vulkan, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < vulkan->memory_properties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && ((vulkan->memory_properties.memoryTypes[i].propertyFlags & properties) == properties)) {
            return i;
        }
    }
    return (uint32_t)-1;  // Not found
}

int create_vulkan_image_and_export_fd(struct pwc_vulkan *vulkan, int *fd_out, VkImage *image_out) {
    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = vulkan->image_format,  // Используем формат из вашего кода, например VK_FORMAT_R8G8B8A8_SRGB
        .extent = {vulkan->width, vulkan->height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,  // Для рендеринга
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .flags = 0, // VK_IMAGE_CREATE_EXTERNAL_MEMORY_BIT_KHR
    };

    VkImage image;
    if (vkCreateImage(vulkan->device, &imageCreateInfo, NULL, &image) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan image\n");
        return EXIT_FAILURE;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vulkan->device, image, &memRequirements);

    VkExportMemoryAllocateInfo exportInfo = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = find_memory_type(vulkan, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        .pNext = &exportInfo,
    };

    VkDeviceMemory memory;
    if (vkAllocateMemory(vulkan->device, &allocInfo, NULL, &memory) != VK_SUCCESS) {
        vkDestroyImage(vulkan->device, image, NULL);
        fprintf(stderr, "Failed to allocate memory for image\n");
        return EXIT_FAILURE;
    }

    vkBindImageMemory(vulkan->device, image, memory, 0);

    // Экспортируем FD
    VkMemoryGetFdInfoKHR getFdInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };

    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHRFunc = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(vulkan->device, "vkGetMemoryFdKHR");
    if (!vkGetMemoryFdKHRFunc) {
        fprintf(stderr, "Failed to get vkGetMemoryFdKHR function pointer\n");
        vkFreeMemory(vulkan->device, memory, NULL);
        vkDestroyImage(vulkan->device, image, NULL);
        return EXIT_FAILURE;
    }
    if (vkGetMemoryFdKHRFunc(vulkan->device, &getFdInfo, fd_out) != VK_SUCCESS) {  // Вызываем через указатель
        fprintf(stderr, "Failed to export FD from Vulkan memory\n");
        vkFreeMemory(vulkan->device, memory, NULL);
        vkDestroyImage(vulkan->device, image, NULL);
        return EXIT_FAILURE;
    }

    *image_out = image;  // Возвращаем хэндл изображения, если нужно
    return EXIT_SUCCESS;  // fd_out теперь содержит FD
}

VkResult render_red_screen_to_image(struct pwc_vulkan *vulkan, VkImage image) {
    // 1. Создайте staging буфер для красных данных
    VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vulkan->width * vulkan->height * 4,  // 4 байта на пиксель (RGBA)
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkBuffer stagingBuffer;
    if (vkCreateBuffer(vulkan->device, &bufferCreateInfo, NULL, &stagingBuffer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create staging buffer\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vulkan->device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = find_memory_type(vulkan, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };

    VkDeviceMemory stagingMemory;
    if (vkAllocateMemory(vulkan->device, &allocInfo, NULL, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(vulkan->device, stagingBuffer, NULL);
        fprintf(stderr, "Failed to allocate staging memory\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkBindBufferMemory(vulkan->device, stagingBuffer, stagingMemory, 0);

    // 2. Заполните буфер красными данными (R=255, G=0, B=0, A=255)
    void* data;
    vkMapMemory(vulkan->device, stagingMemory, 0, allocInfo.allocationSize, 0, &data);
    uint8_t* byteData = (uint8_t*)data;
    for (uint32_t i = 0; i < vulkan->height * vulkan->width; i++) {
        byteData[i * 4 + 0] = 255;  // R
        byteData[i * 4 + 1] = 0;    // G
        byteData[i * 4 + 2] = 0;    // B
        byteData[i * 4 + 3] = 255;  // A
    }
    vkUnmapMemory(vulkan->device, stagingMemory);

    // 3. Создайте и запишите командный буфер
    VkCommandBufferAllocateInfo allocInfoCmd = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vulkan->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vulkan->device, &allocInfoCmd, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Переход изображения в TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    // Копируйте из буфера в изображение
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,  // Плотная упаковка
        .bufferImageHeight = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent = { vulkan->width, vulkan->height, 1 },
    };
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Переход изображения в FINAL_LAYOUT (например, для отображения)
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Или VK_IMAGE_LAYOUT_GENERAL, в зависимости от использования
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    // 4. Выполните командный буфер
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };
    vkQueueSubmit(vulkan->graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);  // Без fence для простоты
    vkQueueWaitIdle(vulkan->graphics_queue);  // Ждём завершения

    // 5. Очистите ресурсы
    vkFreeCommandBuffers(vulkan->device, vulkan->cmd_pool, 1, &commandBuffer);
    vkDestroyBuffer(vulkan->device, stagingBuffer, NULL);
    vkFreeMemory(vulkan->device, stagingMemory, NULL);

    return VK_SUCCESS;
}

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