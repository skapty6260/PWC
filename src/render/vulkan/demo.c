#include "pwc/render/vulkan/vk-core.h"
#include <assert.h>
#include <pwc/render/utils/linmath.h>
#include <pwc/render/vulkan/demo.h>
#include <pwc/render/vulkan/vulkan.h>
#include <pwc/render/utils/macro.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

static char *tex_files[] = {"lunarg.ppm.h"};

struct vktexcube_vs_uniform {
    // Must start with MVP
    float mvp[4][4];
    float position[12 * 3][4];
    float attr[12 * 3][4];
};

//--------------------------------------------------------------------------------------
// Mesh and VertexFormat Data
//--------------------------------------------------------------------------------------
// clang-format off
static const float g_vertex_buffer_data[] = {
    -1.0f,-1.0f,-1.0f,  // -X side
    -1.0f,-1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,

    -1.0f,-1.0f,-1.0f,  // -Z side
     1.0f, 1.0f,-1.0f,
     1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,
     1.0f, 1.0f,-1.0f,

    -1.0f,-1.0f,-1.0f,  // -Y side
     1.0f,-1.0f,-1.0f,
     1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,
     1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,

    -1.0f, 1.0f,-1.0f,  // +Y side
    -1.0f, 1.0f, 1.0f,
     1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
     1.0f, 1.0f, 1.0f,
     1.0f, 1.0f,-1.0f,

     1.0f, 1.0f,-1.0f,  // +X side
     1.0f, 1.0f, 1.0f,
     1.0f,-1.0f, 1.0f,
     1.0f,-1.0f, 1.0f,
     1.0f,-1.0f,-1.0f,
     1.0f, 1.0f,-1.0f,

    -1.0f, 1.0f, 1.0f,  // +Z side
    -1.0f,-1.0f, 1.0f,
     1.0f, 1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
     1.0f,-1.0f, 1.0f,
     1.0f, 1.0f, 1.0f,
};

static const float g_uv_buffer_data[] = {
    0.0f, 1.0f,  // -X side
    1.0f, 1.0f,
    1.0f, 0.0f,
    1.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 1.0f,

    1.0f, 1.0f,  // -Z side
    0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 0.0f,

    1.0f, 0.0f,  // -Y side
    1.0f, 1.0f,
    0.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 1.0f,
    0.0f, 0.0f,

    1.0f, 0.0f,  // +Y side
    0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,

    1.0f, 0.0f,  // +X side
    0.0f, 0.0f,
    0.0f, 1.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,

    0.0f, 0.0f,  // +Z side
    0.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
};

void dumpMatrix(const char *note, mat4x4 MVP) {
    int i;

    printf("%s: \n", note);
    for (i = 0; i < 4; i++) {
        printf("%f, %f, %f, %f\n", MVP[i][0], MVP[i][1], MVP[i][2], MVP[i][3]);
    }
    printf("\n");
    fflush(stdout);
}

void dumpVec4(const char *note, vec4 vector) {
    printf("%s: \n", note);
    printf("%f, %f, %f, %f\n", vector[0], vector[1], vector[2], vector[3]);
    printf("\n");
    fflush(stdout);
}

// static void demo_name_object(struct pwc_vulkan *vulkan, VkObjectType object_type, uint64_t vulkan_handle, const char *format, ...) {
//     if (!vulkan->validate) return;

//     VkResult U_ASSERT_ONLY err;
//     char name[1024];
//     va_list argptr;
//     va_start(argptr, format);
//     vsnprintf(name, sizeof(name), format, argptr);
//     va_end(argptr);
//     name[sizeof(name) - 1] = '\0';

//     VkDebugUtilsObjectNameInfoEXT obj_name = {
//         .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
//         .pNext = NULL,
//         .objectType = object_type,
//         .objectHandle = vulkan_handle,
//         .pObjectName = name,
//     };
//     err = vkSetDebugUtilsObjectNameEXT(vulkan->device, &obj_name);
//     assert(!err);
// }

#include <pwc/render/demo_tex.ppm.h>
bool loadTexture(const char *filename, uint8_t *rgba_data, VkSubresourceLayout *layout, int32_t *width, int32_t *height) {
    (void)filename;
    char *cPtr;
    cPtr = (char *)lunarg_ppm;
    if ((unsigned char *)cPtr >= (lunarg_ppm + lunarg_ppm_len) || strncmp(cPtr, "P6\n", 3)) {
        return false;
    }
    while (strncmp(cPtr++, "\n", 1));
    sscanf(cPtr, "%u %u", width, height);
    if (rgba_data == NULL) {
        return true;
    }
    while (strncmp(cPtr++, "\n", 1));
    if ((unsigned char *)cPtr >= (lunarg_ppm + lunarg_ppm_len) || strncmp(cPtr, "255\n", 4)) {
        return false;
    }
    while (strncmp(cPtr++, "\n", 1));
    for (int y = 0; y < *height; y++) {
        uint8_t *rowPtr = rgba_data;
        for (int x = 0; x < *width; x++) {
            memcpy(rowPtr, cPtr, 3);
            rowPtr[3] = 255; /* Alpha of 1 */
            rowPtr += 4;
            cPtr += 3;
        }
        rgba_data += layout->rowPitch;
    }
    return true;
}

void demo_init(struct pwc_vulkan *vulkan) {
    vec3 eye = {0.0f, 3.0f, 5.0f};
    vec3 origin = {0, 0, 0};
    vec3 up = {0.0f, 1.0f, 0.0};

    struct pwc_demo *demo = calloc(1, sizeof(struct pwc_demo));
    // vulkan->demo = demo;
    demo->vulkan = vulkan;
    demo->presentMode = VK_PRESENT_MODE_FIFO_KHR;
    demo->demo_frame_count = INT32_MAX;
    demo->width = 500;
    demo->height = 500;

    demo->initialized = false;
    demo->spin_angle = 4.0f;
    demo->spin_increment = 0.2f;
    demo->pause = false;

    mat4x4_perspective(demo->projection_matrix, (float)degreesToRadians(45.0f), 1.0f, 0.1f, 100.0f);
    mat4x4_look_at(demo->view_matrix, eye, origin, up);
    mat4x4_identity(demo->model_matrix);

    demo->projection_matrix[1][1] *= -1;  // Flip projection matrix from GL to Vulkan orientation.
}

static bool memory_type_from_properties(struct pwc_demo *demo, uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((demo->memory_properties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
}

static void demo_prepare_texture_buffer(struct pwc_demo *demo, const char *filename, struct texture_object *tex_obj) {
    int32_t tex_width;
    int32_t tex_height;
    VkResult U_ASSERT_ONLY err;
    bool U_ASSERT_ONLY pass;

    if (!loadTexture(filename, NULL, NULL, &tex_width, &tex_height)) {
        fprintf(stderr, "Failed to load textures");
        exit(EXIT_FAILURE);
    }

    tex_obj->tex_width = tex_width;
    tex_obj->tex_height = tex_height;

    const VkBufferCreateInfo buffer_create_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                   .pNext = NULL,
                                                   .flags = 0,
                                                   .size = tex_width * tex_height * 4,
                                                   .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                   .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                   .queueFamilyIndexCount = 0,
                                                   .pQueueFamilyIndices = NULL};
    
    err = vkCreateBuffer(demo->vulkan->device, &buffer_create_info, NULL, &tex_obj->buffer);
    assert(!err);
    //     demo_name_object(demo, VK_OBJECT_TYPE_BUFFER, (uint64_t)tex_obj->buffer, "TexBuffer(%s)", filename);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(demo->vulkan->device, tex_obj->buffer, &mem_reqs);

    tex_obj->mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    tex_obj->mem_alloc.pNext = NULL;
    tex_obj->mem_alloc.allocationSize = mem_reqs.size;
    tex_obj->mem_alloc.memoryTypeIndex = 0;

    VkFlags requirements = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    pass = memory_type_from_properties(demo, mem_reqs.memoryTypeBits, requirements, &tex_obj->mem_alloc.memoryTypeIndex);
    assert(pass);

    err = vkAllocateMemory(demo->vulkan->device, &tex_obj->mem_alloc, NULL, &(tex_obj->mem));
    assert(!err);
    // demo_name_object(demo->vulkan, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)tex_obj->mem, "TexBufMemory(%s)", filename);

    /* bind memory */
    err = vkBindBufferMemory(demo->vulkan->device, tex_obj->buffer, tex_obj->mem, 0);
    assert(!err);

    VkSubresourceLayout layout;
    memset(&layout, 0, sizeof(layout));
    layout.rowPitch = tex_width * 4;

    void *data;
    err = vkMapMemory(demo->vulkan->device, tex_obj->mem, 0, tex_obj->mem_alloc.allocationSize, 0, &data);
    assert(!err);

    if (!loadTexture(filename, data, &layout, &tex_width, &tex_height)) {
        fprintf(stderr, "Error loading texture: %s\n", filename);
    }

    vkUnmapMemory(demo->vulkan->device, tex_obj->mem);
}

static void demo_prepare_texture_image(struct pwc_demo *demo, const char *filename, struct texture_object *tex_obj, VkImageTiling tiling, VkImageUsageFlags usage, VkFlags required_props) {
     const VkFormat tex_format = VK_FORMAT_R8G8B8A8_SRGB;
    int32_t tex_width;
    int32_t tex_height;
    VkResult U_ASSERT_ONLY err;
    bool U_ASSERT_ONLY pass;

    if (!loadTexture(filename, NULL, NULL, &tex_width, &tex_height)) {
        fprintf(stderr, "Failed to load textures");
        exit(EXIT_FAILURE);
    }

    tex_obj->tex_width = tex_width;
    tex_obj->tex_height = tex_height;

    const VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = tex_format,
        .extent = {tex_width, tex_height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .flags = 0,
        .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
    };

    VkMemoryRequirements mem_reqs;

    err = vkCreateImage(demo->vulkan->device, &image_create_info, NULL, &tex_obj->image);
    assert(!err);
    // demo_name_object(demo, VK_OBJECT_TYPE_IMAGE, (uint64_t)tex_obj->image, "TexImage(%s)", filename);

    vkGetImageMemoryRequirements(demo->vulkan->device, tex_obj->image, &mem_reqs);

    tex_obj->mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    tex_obj->mem_alloc.pNext = NULL;
    tex_obj->mem_alloc.allocationSize = mem_reqs.size;
    tex_obj->mem_alloc.memoryTypeIndex = 0;

    pass = memory_type_from_properties(demo, mem_reqs.memoryTypeBits, required_props, &tex_obj->mem_alloc.memoryTypeIndex);
    assert(pass);

    /* allocate memory */
    err = vkAllocateMemory(demo->vulkan->device, &tex_obj->mem_alloc, NULL, &(tex_obj->mem));
    assert(!err);
    //     demo_name_object(demo, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)tex_obj->mem, "TexImageMem(%s)", filename);

    /* bind memory */
    err = vkBindImageMemory(demo->vulkan->device, tex_obj->image, tex_obj->mem, 0);
    assert(!err);

    if (required_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        const VkImageSubresource subres = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .arrayLayer = 0,
        };
        VkSubresourceLayout layout;
        void *data;

        vkGetImageSubresourceLayout(demo->vulkan->device, tex_obj->image, &subres, &layout);

        err = vkMapMemory(demo->vulkan->device, tex_obj->mem, 0, tex_obj->mem_alloc.allocationSize, 0, &data);
        assert(!err);

        if (!loadTexture(filename, data, &layout, &tex_width, &tex_height)) {
            fprintf(stderr, "Error loading texture: %s\n", filename);
        }

        vkUnmapMemory(demo->vulkan->device, tex_obj->mem);
    }

    tex_obj->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static void demo_set_image_layout(struct pwc_demo *demo, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkImageLayout new_image_layout, VkAccessFlagBits srcAccessMask, VkPipelineStageFlags src_stages, VkPipelineStageFlags dest_stages) {
    assert(demo->vulkan->cmd);

    VkImageMemoryBarrier image_memory_barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                 .pNext = NULL,
                                                 .srcAccessMask = srcAccessMask,
                                                 .dstAccessMask = 0,
                                                 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                 .oldLayout = old_image_layout,
                                                 .newLayout = new_image_layout,
                                                 .image = image,
                                                 .subresourceRange = {aspectMask, 0, 1, 0, 1}};

    switch (new_image_layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            /* Make sure anything that was copying from this image has completed */
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            image_memory_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            break;

        default:
            image_memory_barrier.dstAccessMask = 0;
            break;
    }

    VkImageMemoryBarrier *pmemory_barrier = &image_memory_barrier;

    vkCmdPipelineBarrier(demo->vulkan->cmd, src_stages, dest_stages, 0, 0, NULL, 0, NULL, 1, pmemory_barrier);
}

static void demo_prepare_textures(struct pwc_demo *demo) {
    const VkFormat tex_format = VK_FORMAT_R8G8B8A8_SRGB;
    VkFormatProperties props;
    uint32_t i;

    vkGetPhysicalDeviceFormatProperties(demo->vulkan->physicalDevice, tex_format, &props);

    for (i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        VkResult U_ASSERT_ONLY err;

         if ((props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) && !demo->use_staging_buffer) {
            // demo_push_cb_label(demo, demo->vulkan->cmd, NULL, "DirectTexture(%u)", i);
            /* Device can texture using linear textures */
            demo_prepare_texture_image(demo, tex_files[i], &demo->textures[i], VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            // Nothing in the pipeline needs to be complete to start, and don't allow fragment
            // shader to run until layout transition completes
            demo_set_image_layout(demo, demo->textures[i].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED,
                                  demo->textures[i].imageLayout, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            demo->staging_texture.image = 0;
            // demo_pop_cb_label(demo, demo->vulkan->cmd);  // "DirectTexture"
        } else if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            /* Must use staging buffer to copy linear texture to optimized */
            // demo_push_cb_label(demo, demo->vulkan->cmd, NULL, "StagingTexture(%u)", i);

            memset(&demo->staging_texture, 0, sizeof(demo->staging_texture));
            demo_prepare_texture_buffer(demo, tex_files[i], &demo->staging_texture);

            demo_prepare_texture_image(demo, tex_files[i], &demo->textures[i], VK_IMAGE_TILING_OPTIMAL,
                                       (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            demo_set_image_layout(demo, demo->textures[i].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT);

            // demo_push_cb_label(demo, demo->vulkan->cmd, NULL, "StagingBufferCopy(%u)", i);

            VkBufferImageCopy copy_region = {
                .bufferOffset = 0,
                .bufferRowLength = demo->staging_texture.tex_width,
                .bufferImageHeight = demo->staging_texture.tex_height,
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .imageOffset = {0, 0, 0},
                .imageExtent = {demo->staging_texture.tex_width, demo->staging_texture.tex_height, 1},
            };

            vkCmdCopyBufferToImage(demo->vulkan->cmd, demo->staging_texture.buffer, demo->textures[i].image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
            // demo_pop_cb_label(demo, demo->vulkan->cmd);  // "StagingBufferCopy"

            demo_set_image_layout(demo, demo->textures[i].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  demo->textures[i].imageLayout, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            // demo_pop_cb_label(demo, demo->vulkan->cmd);  // "StagingTexture"
        } else {
            /* Can't support VK_FORMAT_R8G8B8A8_SRGB !? */
            assert(!"No support for R8G8B8A8_SRGB as texture image format");
        }

         const VkSamplerCreateInfo sampler = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = NULL,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VkImageViewCreateInfo view = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .image = VK_NULL_HANDLE,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = tex_format,
            .components =
                {
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .flags = 0,
        };

        /* create sampler */
        err = vkCreateSampler(demo->vulkan->device, &sampler, NULL, &demo->textures[i].sampler);
        assert(!err);
        // demo_name_object(demo, VK_OBJECT_TYPE_SAMPLER, (uint64_t)demo->textures[i].sampler, "Sampler(%u)", i);

        /* create image view */
        view.image = demo->textures[i].image;
        err = vkCreateImageView(demo->vulkan->device, &view, NULL, &demo->textures[i].view);
        // demo_name_object(demo->vulkan, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)demo->textures[i].view, "TexImageView(%u)", i);
        assert(!err);
    }
}

static void demo_destroy_texture(struct pwc_demo *demo, struct texture_object *tex_objs) {
    /* clean up staging resources */
    vkFreeMemory(demo->vulkan->device, tex_objs->mem, NULL);
    if (tex_objs->image) vkDestroyImage(demo->vulkan->device, tex_objs->image, NULL);
    if (tex_objs->buffer) vkDestroyBuffer(demo->vulkan->device, tex_objs->buffer, NULL);
}

static void demo_prepare_cube_data_buffers(struct pwc_demo *demo) {
    VkBufferCreateInfo buf_info;
    VkMemoryRequirements mem_reqs;
    VkMemoryAllocateInfo mem_alloc;
    mat4x4 MVP, VP;
    VkResult U_ASSERT_ONLY err;
    bool U_ASSERT_ONLY pass;
    struct vktexcube_vs_uniform data;

    mat4x4_mul(VP, demo->projection_matrix, demo->view_matrix);
    mat4x4_mul(MVP, VP, demo->model_matrix);
    memcpy(data.mvp, MVP, sizeof(MVP));

    for (unsigned int i = 0; i < 12 * 3; i++) {
        data.position[i][0] = g_vertex_buffer_data[i * 3];
        data.position[i][1] = g_vertex_buffer_data[i * 3 + 1];
        data.position[i][2] = g_vertex_buffer_data[i * 3 + 2];
        data.position[i][3] = 1.0f;
        data.attr[i][0] = g_uv_buffer_data[2 * i];
        data.attr[i][1] = g_uv_buffer_data[2 * i + 1];
        data.attr[i][2] = 0;
        data.attr[i][3] = 0;
    }

    memset(&buf_info, 0, sizeof(buf_info));
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buf_info.size = sizeof(data);

    for (unsigned int i = 0; i < FRAME_LAG; i++) {
        err = vkCreateBuffer(demo->vulkan->device, &buf_info, NULL, &demo->submission_resources[i].uniform_buffer);
        assert(!err);
        // demo_name_object(demo->vulkan, VK_OBJECT_TYPE_BUFFER, (uint64_t)demo->submission_resources[i].uniform_buffer, "UniformBuf(%u)", i);

        vkGetBufferMemoryRequirements(demo->vulkan->device, demo->submission_resources[i].uniform_buffer, &mem_reqs);

        mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mem_alloc.pNext = NULL;
        mem_alloc.allocationSize = mem_reqs.size;
        mem_alloc.memoryTypeIndex = 0;

        pass = memory_type_from_properties(demo, mem_reqs.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                           &mem_alloc.memoryTypeIndex);
        assert(pass);

        err = vkAllocateMemory(demo->vulkan->device, &mem_alloc, NULL, &demo->submission_resources[i].uniform_memory);
        assert(!err);
        // demo_name_object(demo->vulkan, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)demo->submission_resources[i].uniform_memory,
                        //  "UniformMem(%u)", i);

        err = vkMapMemory(demo->vulkan->device, demo->submission_resources[i].uniform_memory, 0, VK_WHOLE_SIZE, 0,
                          &demo->submission_resources[i].uniform_memory_ptr);
        assert(!err);

        memcpy(demo->submission_resources[i].uniform_memory_ptr, &data, sizeof data);

        err = vkBindBufferMemory(demo->vulkan->device, demo->submission_resources[i].uniform_buffer,
                                 demo->submission_resources[i].uniform_memory, 0);
        assert(!err);
    }
}

static void demo_prepare_descriptor_layout(struct pwc_demo *demo) {
    const VkDescriptorSetLayoutBinding layout_bindings[2] = {
        [0] =
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = NULL,
            },
        [1] =
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = DEMO_TEXTURE_COUNT,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL,
            },
    };
    const VkDescriptorSetLayoutCreateInfo descriptor_layout = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .bindingCount = 2,
        .pBindings = layout_bindings,
    };
    VkResult U_ASSERT_ONLY err;

    err = vkCreateDescriptorSetLayout(demo->vulkan->device, &descriptor_layout, NULL, &demo->desc_layout);
    assert(!err);

    const VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = 1,
        .pSetLayouts = &demo->desc_layout,
    };

    err = vkCreatePipelineLayout(demo->vulkan->device, &pPipelineLayoutCreateInfo, NULL, &demo->pipeline_layout);
    assert(!err);
}

static void demo_prepare_render_pass(struct pwc_demo *demo) {
    // The initial layout for the color and depth attachments will be LAYOUT_UNDEFINED
    // because at the start of the renderpass, we don't care about their contents.
    // At the start of the subpass, the color attachment's layout will be transitioned
    // to LAYOUT_COLOR_ATTACHMENT_OPTIMAL and the depth stencil attachment's layout
    // will be transitioned to LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL.  At the end of
    // the renderpass, the color attachment's layout will be transitioned to
    // LAYOUT_PRESENT_SRC_KHR to be ready to present.  This is all done as part of
    // the renderpass, no barriers are necessary.
    const VkAttachmentDescription attachments[2] = {
        [0] =
            {
                .format = demo->vulkan->format,
                .flags = 0,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
        [1] =
            {
                .format = demo->demo_depth.format,
                .flags = 0,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
    };
    const VkAttachmentReference color_reference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    const VkAttachmentReference depth_reference = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    const VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .flags = 0,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_reference,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = &depth_reference,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    VkSubpassDependency attachmentDependencies[2] = {
        [0] =
            {
                // Depth buffer is shared between swapchain images
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = 0,
            },
        [1] =
            {
                // Image Layout Transition
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                .dependencyFlags = 0,
            },
    };

    const VkRenderPassCreateInfo rp_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = attachmentDependencies,
    };
    VkResult U_ASSERT_ONLY err;

    err = vkCreateRenderPass(demo->vulkan->device, &rp_info, NULL, &demo->render_pass);
    assert(!err);
}

static VkShaderModule demo_prepare_shader_module(const char *name, struct pwc_demo *demo, const uint32_t *code, size_t size) {
    VkShaderModule module;
    VkShaderModuleCreateInfo moduleCreateInfo;
    VkResult U_ASSERT_ONLY err;

    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext = NULL;
    moduleCreateInfo.flags = 0;
    moduleCreateInfo.codeSize = size;
    moduleCreateInfo.pCode = code;

    err = vkCreateShaderModule(demo->vulkan->device, &moduleCreateInfo, NULL, &module);
    assert(!err);
    // demo_name_object(demo, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)module, "%s", name);

    return module;
}

static void demo_prepare_vs(struct pwc_demo *demo) {
    const uint32_t vs_code[] = {
#include <pwc/render/cube.vert.inc>
    };
    demo->vert_shader_module = demo_prepare_shader_module("cube.vert", demo, vs_code, sizeof(vs_code));
}

static void demo_prepare_fs(struct pwc_demo *demo) {
    const uint32_t fs_code[] = {
#include <pwc/render/cube.frag.inc>
    };
    demo->frag_shader_module = demo_prepare_shader_module("cube.frag", demo, fs_code, sizeof(fs_code));
}

static void demo_prepare_pipeline(struct pwc_demo *demo) {
#define NUM_DYNAMIC_STATES 2 /*Viewport + Scissor*/

    VkGraphicsPipelineCreateInfo pipeline;
    VkPipelineCacheCreateInfo pipelineCache;
    VkPipelineVertexInputStateCreateInfo vi;
    VkPipelineInputAssemblyStateCreateInfo ia;
    VkPipelineRasterizationStateCreateInfo rs;
    VkPipelineColorBlendStateCreateInfo cb;
    VkPipelineDepthStencilStateCreateInfo ds;
    VkPipelineViewportStateCreateInfo vp;
    VkPipelineMultisampleStateCreateInfo ms;
    VkDynamicState dynamicStateEnables[NUM_DYNAMIC_STATES];
    VkPipelineDynamicStateCreateInfo dynamicState;
    VkResult U_ASSERT_ONLY err;

    memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
    memset(&dynamicState, 0, sizeof dynamicState);
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables;

    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.layout = demo->pipeline_layout;

    memset(&vi, 0, sizeof(vi));
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    memset(&rs, 0, sizeof(rs));
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.depthBiasEnable = VK_FALSE;
    rs.lineWidth = 1.0f;

    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    VkPipelineColorBlendAttachmentState att_state[1];
    memset(att_state, 0, sizeof(att_state));
    att_state[0].colorWriteMask = 0xf;
    att_state[0].blendEnable = VK_FALSE;
    cb.attachmentCount = 1;
    cb.pAttachments = att_state;

    memset(&vp, 0, sizeof(vp));
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
    vp.scissorCount = 1;
    dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;

    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.stencilTestEnable = VK_FALSE;
    ds.front = ds.back;

    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.pSampleMask = NULL;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    demo_prepare_vs(demo);
    demo_prepare_fs(demo);

    // Two stages: vs and fs
    VkPipelineShaderStageCreateInfo shaderStages[2];
    memset(&shaderStages, 0, 2 * sizeof(VkPipelineShaderStageCreateInfo));

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = demo->vert_shader_module;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = demo->frag_shader_module;
    shaderStages[1].pName = "main";

    memset(&pipelineCache, 0, sizeof(pipelineCache));
    pipelineCache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    err = vkCreatePipelineCache(demo->vulkan->device, &pipelineCache, NULL, &demo->pipelineCache);
    assert(!err);

    pipeline.pVertexInputState = &vi;
    pipeline.pInputAssemblyState = &ia;
    pipeline.pRasterizationState = &rs;
    pipeline.pColorBlendState = &cb;
    pipeline.pMultisampleState = &ms;
    pipeline.pViewportState = &vp;
    pipeline.pDepthStencilState = &ds;
    pipeline.stageCount = ARRAY_SIZE(shaderStages);
    pipeline.pStages = shaderStages;
    pipeline.renderPass = demo->render_pass;
    pipeline.pDynamicState = &dynamicState;

    err = vkCreateGraphicsPipelines(demo->vulkan->device, demo->pipelineCache, 1, &pipeline, NULL, &demo->pipeline);
    assert(!err);

    vkDestroyShaderModule(demo->vulkan->device, demo->frag_shader_module, NULL);
    vkDestroyShaderModule(demo->vulkan->device, demo->vert_shader_module, NULL);
}

static void demo_prepare_descriptor_pool(struct pwc_demo *demo) {
    const VkDescriptorPoolSize type_counts[2] = {
        [0] =
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = FRAME_LAG,
            },
        [1] =
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = FRAME_LAG * DEMO_TEXTURE_COUNT,
            },
    };
    const VkDescriptorPoolCreateInfo descriptor_pool = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .maxSets = FRAME_LAG,
        .poolSizeCount = 2,
        .pPoolSizes = type_counts,
    };
    VkResult U_ASSERT_ONLY err;

    err = vkCreateDescriptorPool(demo->vulkan->device, &descriptor_pool, NULL, &demo->desc_pool);
    assert(!err);
}

static void demo_prepare_descriptor_set(struct pwc_demo *demo) {
    VkDescriptorImageInfo tex_descs[DEMO_TEXTURE_COUNT];
    VkWriteDescriptorSet writes[2];
    VkResult U_ASSERT_ONLY err;

    VkDescriptorSetAllocateInfo alloc_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .pNext = NULL,
                                              .descriptorPool = demo->desc_pool,
                                              .descriptorSetCount = 1,
                                              .pSetLayouts = &demo->desc_layout};

    VkDescriptorBufferInfo buffer_info;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(struct vktexcube_vs_uniform);

    memset(&tex_descs, 0, sizeof(tex_descs));
    for (unsigned int i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        tex_descs[i].sampler = demo->textures[i].sampler;
        tex_descs[i].imageView = demo->textures[i].view;
        tex_descs[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    memset(&writes, 0, sizeof(writes));

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &buffer_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = DEMO_TEXTURE_COUNT;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = tex_descs;

    for (unsigned int i = 0; i < FRAME_LAG; i++) {
        err = vkAllocateDescriptorSets(demo->vulkan->device, &alloc_info, &demo->submission_resources[i].descriptor_set);
        assert(!err);
        buffer_info.buffer = demo->submission_resources[i].uniform_buffer;
        writes[0].dstSet = demo->submission_resources[i].descriptor_set;
        writes[1].dstSet = demo->submission_resources[i].descriptor_set;
        vkUpdateDescriptorSets(demo->vulkan->device, 2, writes, 0, NULL);
    }
}

static void demo_prepare_submission_sync_objects(struct pwc_demo *demo) {
    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    // Create fences that we can use to throttle if we get too far
    // ahead of the image presents
    VkFenceCreateInfo fence_ci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = NULL, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    VkResult U_ASSERT_ONLY err;
    for (uint32_t i = 0; i < FRAME_LAG; i++) {
        err = vkCreateFence(demo->vulkan->device, &fence_ci, NULL, &demo->submission_resources[i].fence);
        assert(!err);

        err = vkCreateSemaphore(demo->vulkan->device, &semaphoreCreateInfo, NULL, &demo->submission_resources[i].image_acquired_semaphore);
        assert(!err);
        // demo_name_object(demo, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)demo->submission_resources[i].image_acquired_semaphore,
        //                  "AcquireSem(%u)", i);
    }
}

static void demo_flush_init_cmd(struct pwc_demo *demo) {
    VkResult U_ASSERT_ONLY err;

    // This function could get called twice if the texture uses a staging buffer
    // In that case the second call should be ignored
    if (demo->vulkan->cmd == VK_NULL_HANDLE) return;

    err = vkEndCommandBuffer(demo->vulkan->cmd);
    assert(!err);

    VkFence fence;
    VkFenceCreateInfo fence_ci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = NULL, .flags = 0};
    // if (demo->force_errors) {
    //     // Remove sType to intentionally force validation layer errors.
    //     fence_ci.sType = 0;
    // }
    err = vkCreateFence(demo->vulkan->device, &fence_ci, NULL, &fence);
    assert(!err);
    // demo_name_object(demo->vulkan, VK_OBJECT_TYPE_FENCE, (uint64_t)fence, "InitFence");

    const VkCommandBuffer cmd_bufs[] = {demo->vulkan->cmd};
    VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .pNext = NULL,
                                .waitSemaphoreCount = 0,
                                .pWaitSemaphores = NULL,
                                .pWaitDstStageMask = NULL,
                                .commandBufferCount = 1,
                                .pCommandBuffers = cmd_bufs,
                                .signalSemaphoreCount = 0,
                                .pSignalSemaphores = NULL};

    err = vkQueueSubmit(demo->vulkan->graphics_queue, 1, &submit_info, fence);
    assert(!err);

    err = vkWaitForFences(demo->vulkan->device, 1, &fence, VK_TRUE, UINT64_MAX);
    assert(!err);

    vkFreeCommandBuffers(demo->vulkan->device, demo->vulkan->cmd_pool, 1, cmd_bufs);
    vkDestroyFence(demo->vulkan->device, fence, NULL);
    demo->vulkan->cmd = VK_NULL_HANDLE;
}

static void demo_prepare_depth(struct pwc_demo *demo) {
    demo->demo_depth.format = VK_FORMAT_D16_UNORM;

    const VkImageCreateInfo image = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = demo->demo_depth.format,
        .extent = {demo->width, demo->height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .flags = 0,
    };

    VkImageViewCreateInfo view = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .image = VK_NULL_HANDLE,
        .format = demo->demo_depth.format,
        .subresourceRange =
            {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
        .flags = 0,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
    };

    // if (demo->force_errors) {
    //     // Intentionally force a bad pNext value to generate a validation layer error
    //     view.pNext = &image;
    // }

    VkMemoryRequirements mem_reqs;
    VkResult U_ASSERT_ONLY err;
    bool U_ASSERT_ONLY pass;

    /* create image */
    err = vkCreateImage(demo->vulkan->device, &image, NULL, &demo->demo_depth.image);
    assert(!err);
    // demo_name_object(demo, VK_OBJECT_TYPE_IMAGE, (uint64_t)demo->depth.image, "DepthImage");

    vkGetImageMemoryRequirements(demo->vulkan->device, demo->demo_depth.image, &mem_reqs);
    assert(!err);

    demo->demo_depth.mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    demo->demo_depth.mem_alloc.pNext = NULL;
    demo->demo_depth.mem_alloc.allocationSize = mem_reqs.size;
    demo->demo_depth.mem_alloc.memoryTypeIndex = 0;

    pass = memory_type_from_properties(demo, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       &demo->demo_depth.mem_alloc.memoryTypeIndex);
    assert(pass);

    /* allocate memory */
    err = vkAllocateMemory(demo->vulkan->device, &demo->demo_depth.mem_alloc, NULL, &demo->demo_depth.mem);
    assert(!err);
    // demo_name_object(demo, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)demo->depth.mem, "DepthMem");

    /* bind memory */
    err = vkBindImageMemory(demo->vulkan->device, demo->demo_depth.image, demo->demo_depth.mem, 0);
    assert(!err);

    /* create image view */
    view.image = demo->demo_depth.image;
    err = vkCreateImageView(demo->vulkan->device, &view, NULL, &demo->demo_depth.view);
    assert(!err);
    // demo_name_object(demo, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)demo->depth.view, "DepthView");
}

static void demo_prepare_framebuffers(struct pwc_demo *demo) {
    VkImageView attachments[2];
    attachments[1] = demo->demo_depth.view;

    const VkFramebufferCreateInfo fb_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .renderPass = demo->render_pass,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .width = demo->width,
        .height = demo->height,
        .layers = 1,
    };
    VkResult U_ASSERT_ONLY err;
    uint32_t i;

    for (i = 0; i < demo->swapchainImageCount; i++) {
        attachments[0] = demo->swapchain_resources[i].view;
        err = vkCreateFramebuffer(demo->vulkan->device, &fb_info, NULL, &demo->swapchain_resources[i].framebuffer);
        assert(!err);
        // demo_name_object(demo, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)demo->swapchain_resources[i].framebuffer, "Framebuffer(%u)",
        //                  i);
    }
}

// Creates the swapchain, swapchain image views, depth buffer, framebuffers, and sempahores.
// This function returns early if it fails to create a swapchain, setting swapchain_ready to false.
static void demo_prepare_swapchain(struct pwc_demo *demo) {
    VkResult U_ASSERT_ONLY err;
    VkSwapchainKHR oldSwapchain = demo->vulkan->swapchain;

    // Check the surface capabilities and formats
    VkSurfaceCapabilitiesKHR surfCapabilities;
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(demo->vulkan->physicalDevice, demo->vulkan->surface, &surfCapabilities);
    assert(!err);

    uint32_t presentModeCount;
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(demo->vulkan->physicalDevice, demo->vulkan->surface, &presentModeCount, NULL);
    assert(!err);
    VkPresentModeKHR *presentModes = (VkPresentModeKHR *)malloc(presentModeCount * sizeof(VkPresentModeKHR));
    assert(presentModes);
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(demo->vulkan->physicalDevice, demo->vulkan->surface, &presentModeCount, presentModes);
    assert(!err);

    VkExtent2D swapchainExtent;
    // width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
    if (surfCapabilities.currentExtent.width == 0xFFFFFFFF) {
        // If the surface size is undefined, the size is set to the size
        // of the images requested, which must fit within the minimum and
        // maximum values.
        swapchainExtent.width = demo->width;
        swapchainExtent.height = demo->height;

        if (swapchainExtent.width < surfCapabilities.minImageExtent.width) {
            swapchainExtent.width = surfCapabilities.minImageExtent.width;
        } else if (swapchainExtent.width > surfCapabilities.maxImageExtent.width) {
            swapchainExtent.width = surfCapabilities.maxImageExtent.width;
        }

        if (swapchainExtent.height < surfCapabilities.minImageExtent.height) {
            swapchainExtent.height = surfCapabilities.minImageExtent.height;
        } else if (swapchainExtent.height > surfCapabilities.maxImageExtent.height) {
            swapchainExtent.height = surfCapabilities.maxImageExtent.height;
        }
    } else {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfCapabilities.currentExtent;
        demo->width = surfCapabilities.currentExtent.width;
        demo->height = surfCapabilities.currentExtent.height;
    }

    // The FIFO present mode is guaranteed by the spec to be supported
    // and to have no tearing.  It's a great default present mode to use.
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    //  There are times when you may wish to use another present mode.  The
    //  following code shows how to select them, and the comments provide some
    //  reasons you may wish to use them.
    //
    // It should be noted that Vulkan 1.0 doesn't provide a method for
    // synchronizing rendering with the presentation engine's display.  There
    // is a method provided for throttling rendering with the display, but
    // there are some presentation engines for which this method will not work.
    // If an application doesn't throttle its rendering, and if it renders much
    // faster than the refresh rate of the display, this can waste power on
    // mobile devices.  That is because power is being spent rendering images
    // that may never be seen.

    // VK_PRESENT_MODE_IMMEDIATE_KHR is for applications that don't care about
    // tearing, or have some way of synchronizing their rendering with the
    // display.
    // VK_PRESENT_MODE_MAILBOX_KHR may be useful for applications that
    // generally render a new presentable image every refresh cycle, but are
    // occasionally early.  In this case, the application wants the new image
    // to be displayed instead of the previously-queued-for-presentation image
    // that has not yet been displayed.
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR is for applications that generally
    // render a new presentable image every refresh cycle, but are occasionally
    // late.  In this case (perhaps because of stuttering/latency concerns),
    // the application wants the late image to be immediately displayed, even
    // though that may mean some tearing.

    if (demo->presentMode != swapchainPresentMode) {
        for (size_t i = 0; i < presentModeCount; ++i) {
            if (presentModes[i] == demo->presentMode) {
                swapchainPresentMode = demo->presentMode;
                break;
            }
        }
    }
    if (swapchainPresentMode != demo->presentMode) {
        fprintf(stderr, "Present mode specified is not supported\n");
        exit(EXIT_FAILURE);
    }

    // Determine the number of VkImages to use in the swap chain.
    // Application desires to acquire 3 images at a time for triple
    // buffering
    uint32_t desiredNumOfSwapchainImages = 3;
    if (desiredNumOfSwapchainImages < surfCapabilities.minImageCount) {
        desiredNumOfSwapchainImages = surfCapabilities.minImageCount;
    }
    // If maxImageCount is 0, we can ask for as many images as we want;
    // otherwise we're limited to maxImageCount
    if ((surfCapabilities.maxImageCount > 0) && (desiredNumOfSwapchainImages > surfCapabilities.maxImageCount)) {
        // Application must settle for fewer images than desired:
        desiredNumOfSwapchainImages = surfCapabilities.maxImageCount;
    }

    VkSurfaceTransformFlagsKHR preTransform;
    if (surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfCapabilities.currentTransform;
    }

    // Find a supported composite alpha mode - one of these is guaranteed to be set
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[4] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (uint32_t i = 0; i < ARRAY_SIZE(compositeAlphaFlags); i++) {
        if (surfCapabilities.supportedCompositeAlpha & compositeAlphaFlags[i]) {
            compositeAlpha = compositeAlphaFlags[i];
            break;
        }
    }

    VkSwapchainCreateInfoKHR swapchain_ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .surface = demo->vulkan->surface,
        .minImageCount = desiredNumOfSwapchainImages,
        .imageFormat = demo->vulkan->format,
        .imageColorSpace = demo->vulkan->color_space,
        .imageExtent =
            {
                .width = swapchainExtent.width,
                .height = swapchainExtent.height,
            },
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = preTransform,
        .compositeAlpha = compositeAlpha,
        .imageArrayLayers = 1,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .presentMode = swapchainPresentMode,
        .oldSwapchain = oldSwapchain,
        .clipped = true,
    };
    uint32_t i;
    err = vkCreateSwapchainKHR(demo->vulkan->device, &swapchain_ci, NULL, &demo->vulkan->swapchain);
    assert(!err);

    // If we just re-created an existing swapchain, we should destroy the old
    // swapchain at this point.
    // Note: destroying the swapchain also cleans up all its associated
    // presentable images once the platform is done with them.
    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(demo->vulkan->device, oldSwapchain, NULL);
    }

    err = vkGetSwapchainImagesKHR(demo->vulkan->device, demo->vulkan->swapchain, &demo->swapchainImageCount, NULL);
    assert(!err);

    VkImage *swapchainImages = (VkImage *)malloc(demo->swapchainImageCount * sizeof(VkImage));
    assert(swapchainImages);
    err = vkGetSwapchainImagesKHR(demo->vulkan->device, demo->vulkan->swapchain, &demo->swapchainImageCount, swapchainImages);
    assert(!err);

    for (i = 0; i < demo->swapchainImageCount; i++) {
        // demo_name_object(demo, VK_OBJECT_TYPE_IMAGE, (uint64_t)swapchainImages[i], "SwapchainImage(%u)", i);
    }
    for (i = 0; i < demo->swapchainImageCount; i++) {
        VkImageViewCreateInfo color_image_view = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .format = demo->vulkan->format,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .flags = 0,
        };

        demo->swapchain_resources[i].image = swapchainImages[i];

        color_image_view.image = demo->swapchain_resources[i].image;

        err = vkCreateImageView(demo->vulkan->device, &color_image_view, NULL, &demo->swapchain_resources[i].view);
        assert(!err);
        // demo_name_object(demo, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)demo->swapchain_resources[i].view, "SwapchainView(%u)", i);
    }

    // Create semaphores to synchronize acquiring presentable buffers before
    // rendering and waiting for drawing to be complete before presenting
    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    for (i = 0; i < demo->swapchainImageCount; i++) {
        err = vkCreateSemaphore(demo->vulkan->device, &semaphoreCreateInfo, NULL, &demo->swapchain_resources[i].draw_complete_semaphore);
        assert(!err);
        // demo_name_object(demo, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)demo->swapchain_resources[i].draw_complete_semaphore,
        //                  "DrawCompleteSem(%u)", i);

        if (demo->vulkan->separate_present_queue) {
            err = vkCreateSemaphore(demo->vulkan->device, &semaphoreCreateInfo, NULL,
                                    &demo->swapchain_resources[i].image_ownership_semaphore);
            assert(!err);
            // demo_name_object(demo, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)demo->swapchain_resources[i].image_ownership_semaphore,
            //                  "ImageOwnerSem(%u)", i);
        }
    }

    // if (demo->VK_GOOGLE_display_timing_enabled) {
    //     VkRefreshCycleDurationGOOGLE rc_dur;
    //     err = vkGetRefreshCycleDurationGOOGLE(demo->vulkan->device, demo->vulkan->swapchain, &rc_dur);
    //     assert(!err);
    //     demo->refresh_duration = rc_dur.refreshDuration;

    //     demo->syncd_with_actual_presents = false;
    //     // Initially target 1X the refresh duration:
    //     demo->target_IPD = demo->refresh_duration;
    //     demo->refresh_duration_multiplier = 1;
    //     demo->prev_desired_present_time = 0;
    //     demo->next_present_id = 1;
    // }

    if (NULL != swapchainImages) {
        free(swapchainImages);
    }

    if (NULL != presentModes) {
        free(presentModes);
    }

    demo_prepare_depth(demo);
    demo_prepare_framebuffers(demo);
    demo->swapchain_ready = true;
    demo->first_swapchain_frame = true;
}

void demo_prepare(struct pwc_vulkan *vulkan) {
    VkResult U_ASSERT_ONLY err;
    struct pwc_demo *demo = NULL; // vulkan->demo
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
        .commandBufferCount = 1
    };
    err = vkAllocateCommandBuffers(vulkan->device, &cmd, &vulkan->cmd);
    assert(!err);
    // demo_name_object(demo, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)demo->cmd, "PrepareCB");
    VkCommandBufferBeginInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL
    };
    err = vkBeginCommandBuffer(vulkan->cmd, &cmd_buf_info);
    // demo_push_cb_label(demo, demo->cmd, NULL, "Prepare");
    assert(!err);

    demo_prepare_textures(demo);
    demo_prepare_cube_data_buffers(demo);

    demo_prepare_descriptor_layout(demo);

    // Only need to know the format of the depth buffer before we create the renderpass
    demo->demo_depth.format = VK_FORMAT_D16_UNORM;
    demo_prepare_render_pass(demo);
    demo_prepare_pipeline(demo);

    for (uint32_t i = 0; i < FRAME_LAG; i++) {
        err = vkAllocateCommandBuffers(demo->vulkan->device, &cmd, &demo->submission_resources[i].cmd);
        assert(!err);
        // demo_name_object(demo, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)demo->submission_resources[i].cmd, "MainCommandBuffer(%u)",
        //                  i);
    }

    if (demo->vulkan->separate_present_queue) {
        const VkCommandPoolCreateInfo present_cmd_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = demo->vulkan->present_queue_family_index,
            .flags = 0,
        };
        err = vkCreateCommandPool(demo->vulkan->device, &present_cmd_pool_info, NULL, &demo->vulkan->present_cmd_pool);
        assert(!err);
        const VkCommandBufferAllocateInfo present_cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = demo->vulkan->present_cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        for (uint32_t i = 0; i < FRAME_LAG; i++) {
            err = vkAllocateCommandBuffers(demo->vulkan->device, &present_cmd_info, &demo->submission_resources[i].graphics_to_present_cmd);
            assert(!err);
            // demo_name_object(demo, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)demo->submission_resources[i].graphics_to_present_cmd,
            //                  "GfxToPresent(%u)", i);
        }
    }

    demo_prepare_descriptor_pool(demo);
    demo_prepare_descriptor_set(demo);

    demo_prepare_submission_sync_objects(demo);

    /*
     * Prepare functions above may generate pipeline commands
     * that need to be flushed before beginning the render loop.
     */
    // demo_pop_cb_label(demo, demo->vulkan->cmd);  // "Prepare"
    demo_flush_init_cmd(demo);
    if (demo->staging_texture.buffer) {
        demo_destroy_texture(demo, &demo->staging_texture);
    }
    demo->current_submission_index = 0;
    demo->initialized = true;
    demo_prepare_swapchain(demo);
}

void demo_update_data_buffer(struct pwc_demo *demo, void *uniform_memory_ptr) {
    mat4x4 MVP, Model, VP;
    int matrixSize = sizeof(MVP);

    mat4x4_mul(VP, demo->projection_matrix, demo->view_matrix);

    // Rotate around the Y axis
    mat4x4_dup(Model, demo->model_matrix);
    mat4x4_rotate_Y(demo->model_matrix, Model, (float)degreesToRadians(demo->spin_angle));
    mat4x4_orthonormalize(demo->model_matrix, demo->model_matrix);
    mat4x4_mul(MVP, VP, demo->model_matrix);

    memcpy(uniform_memory_ptr, (const void *)&MVP[0][0], matrixSize);
}

static void demo_draw_build_cmd(struct pwc_demo *demo, SubmissionResources *submission_resource,
                                SwapchainImageResources *swapchain_resource) {
    const VkCommandBufferBeginInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        .pInheritanceInfo = NULL,
    };
    const VkClearValue clear_values[2] = {
        [0] = {.color.float32 = {0.2f, 0.2f, 0.2f, 0.2f}},
        [1] = {.depthStencil = {1.0f, 0}},
    };
    const VkRenderPassBeginInfo rp_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = demo->render_pass,
        .framebuffer = swapchain_resource->framebuffer,
        .renderArea.offset.x = 0,
        .renderArea.offset.y = 0,
        .renderArea.extent.width = demo->width,
        .renderArea.extent.height = demo->height,
        .clearValueCount = 2,
        .pClearValues = clear_values,
    };
    VkResult U_ASSERT_ONLY err;

    err = vkResetCommandBuffer(submission_resource->cmd, 0 /* VK_COMMAND_BUFFER_RESET_FLAGS */);
    err = vkBeginCommandBuffer(submission_resource->cmd, &cmd_buf_info);

    // demo_name_object(demo, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)submission_resource->cmd, "CubeDrawCommandBuf");

    // const float begin_color[4] = {0.4f, 0.3f, 0.2f, 0.1f};
    // demo_push_cb_label(demo, submission_resource->cmd, begin_color, "DrawBegin");

    assert(!err);
    vkCmdBeginRenderPass(submission_resource->cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    // const float renderpass_color[4] = {8.4f, 7.3f, 6.2f, 7.1f};
    // demo_push_cb_label(demo, submission_resource->cmd, renderpass_color, "InsideRenderPass");

    vkCmdBindPipeline(submission_resource->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, demo->pipeline);
    vkCmdBindDescriptorSets(submission_resource->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, demo->pipeline_layout, 0, 1,
                            &submission_resource->descriptor_set, 0, NULL);
    VkViewport viewport;
    memset(&viewport, 0, sizeof(viewport));
    float viewport_dimension;
    if (demo->width < demo->height) {
        viewport_dimension = (float)demo->width;
        viewport.y = (demo->height - demo->width) / 2.0f;
    } else {
        viewport_dimension = (float)demo->height;
        viewport.x = (demo->width - demo->height) / 2.0f;
    }
    viewport.height = viewport_dimension;
    viewport.width = viewport_dimension;
    viewport.minDepth = (float)0.0f;
    viewport.maxDepth = (float)1.0f;
    vkCmdSetViewport(submission_resource->cmd, 0, 1, &viewport);

    VkRect2D scissor;
    memset(&scissor, 0, sizeof(scissor));
    scissor.extent.width = demo->width;
    scissor.extent.height = demo->height;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    vkCmdSetScissor(submission_resource->cmd, 0, 1, &scissor);

    // const float draw_color[4] = {-0.4f, -0.3f, -0.2f, -0.1f};
    // demo_push_cb_label(demo, submission_resource->cmd, draw_color, "ActualDraw");
    vkCmdDraw(submission_resource->cmd, 12 * 3, 1, 0, 0);
    // demo_pop_cb_label(demo, submission_resource->cmd);

    // Note that ending the renderpass changes the image's layout from
    // COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR
    vkCmdEndRenderPass(submission_resource->cmd);
    // demo_pop_cb_label(demo, submission_resource->cmd);

    if (demo->vulkan->separate_present_queue) {
        // We have to transfer ownership from the graphics queue family to the
        // present queue family to be able to present.  Note that we don't have
        // to transfer from present queue family back to graphics queue family at
        // the start of the next frame because we don't care about the image's
        // contents at that point.
        VkImageMemoryBarrier image_ownership_barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                        .pNext = NULL,
                                                        .srcAccessMask = 0,
                                                        .dstAccessMask = 0,
                                                        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                        .srcQueueFamilyIndex = demo->vulkan->graphics_queue_family_index,
                                                        .dstQueueFamilyIndex = demo->vulkan->present_queue_family_index,
                                                        .image = swapchain_resource->image,
                                                        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

        vkCmdPipelineBarrier(submission_resource->cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, NULL, 0, NULL, 1, &image_ownership_barrier);
    }
    // demo_pop_cb_label(demo, submission_resource->cmd);
    err = vkEndCommandBuffer(submission_resource->cmd);
    assert(!err);
}

void demo_build_image_ownership_cmd(struct pwc_demo *demo, SubmissionResources *submission_resource,
                                    SwapchainImageResources *swapchain_resource) {
    VkResult U_ASSERT_ONLY err;

    err = vkResetCommandBuffer(submission_resource->graphics_to_present_cmd, 0 /* VK_COMMAND_BUFFER_RESET_FLAGS */);
    assert(!err);

    const VkCommandBufferBeginInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        .pInheritanceInfo = NULL,
    };
    err = vkBeginCommandBuffer(submission_resource->graphics_to_present_cmd, &cmd_buf_info);
    assert(!err);

    VkImageMemoryBarrier image_ownership_barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                    .pNext = NULL,
                                                    .srcAccessMask = 0,
                                                    .dstAccessMask = 0,
                                                    .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                    .srcQueueFamilyIndex = demo->vulkan->graphics_queue_family_index,
                                                    .dstQueueFamilyIndex = demo->vulkan->present_queue_family_index,
                                                    .image = swapchain_resource->image,
                                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    vkCmdPipelineBarrier(submission_resource->graphics_to_present_cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &image_ownership_barrier);
    err = vkEndCommandBuffer(submission_resource->graphics_to_present_cmd);
    assert(!err);
}

static void demo_draw(struct pwc_demo *demo) {
    // Don't draw if initialization isn't complete, if the swapchain became outdated, or if the window is minimized
    if (!demo->initialized || !demo->swapchain_ready) {
        return;
    }

    VkResult U_ASSERT_ONLY err;

    SubmissionResources current_submission = demo->submission_resources[demo->current_submission_index];

    // Ensure no more than FRAME_LAG renderings are outstanding
    vkWaitForFences(demo->vulkan->device, 1, &current_submission.fence, VK_TRUE, UINT64_MAX);

    uint32_t current_swapchain_image_index;
    do {
        // Get the index of the next available swapchain image:
        err = vkAcquireNextImageKHR(demo->vulkan->device, demo->vulkan->swapchain, UINT64_MAX, current_submission.image_acquired_semaphore,
                                    VK_NULL_HANDLE, &current_swapchain_image_index);

        assert(!err);

        // Stop drawing if we resized but didn't successfully create a new swapchain.
        if (!demo->swapchain_ready) {
            return;
        }
    } while (err != VK_SUCCESS);

    SwapchainImageResources current_swapchain_resource = demo->swapchain_resources[current_swapchain_image_index];

    demo_update_data_buffer(demo, current_submission.uniform_memory_ptr);

    demo_draw_build_cmd(demo, &current_submission, &current_swapchain_resource);

    if (demo->vulkan->separate_present_queue) {
        demo_build_image_ownership_cmd(demo, &current_submission, &current_swapchain_resource);
    }

    // Only reset right before submitting so we can't deadlock on an un-signalled fence that has nothing submitted to it
    vkResetFences(demo->vulkan->device, 1, &current_submission.fence);

    // Wait for the image acquired semaphore to be signaled to ensure
    // that the image won't be rendered to until the presentation
    // engine has fully released ownership to the application, and it is
    // okay to render to the image.
    VkPipelineStageFlags pipe_stage_flags;
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit_info.pWaitDstStageMask = &pipe_stage_flags;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &current_submission.image_acquired_semaphore;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &current_submission.cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &current_swapchain_resource.draw_complete_semaphore;
    err = vkQueueSubmit(demo->vulkan->graphics_queue, 1, &submit_info, current_submission.fence);
    assert(!err);

    if (demo->vulkan->separate_present_queue) {
        // If we are using separate queues, change image ownership to the
        // present queue before presenting, waiting for the draw complete
        // semaphore and signalling the ownership released semaphore when finished
        VkFence nullFence = VK_NULL_HANDLE;
        pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &current_swapchain_resource.draw_complete_semaphore;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &current_submission.graphics_to_present_cmd;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &current_swapchain_resource.image_ownership_semaphore;
        err = vkQueueSubmit(demo->vulkan->present_queue, 1, &submit_info, nullFence);
        assert(!err);
    }

    // If we are using separate queues we have to wait for image ownership,
    // otherwise wait for draw complete
    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = (demo->vulkan->separate_present_queue) ? &current_swapchain_resource.image_ownership_semaphore
                                                          : &current_swapchain_resource.draw_complete_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &demo->vulkan->swapchain,
        .pImageIndices = &current_swapchain_image_index,
    };

    // VkRectLayerKHR rect;
    // VkPresentRegionKHR region;
    // VkPresentRegionsKHR regions;

    err = vkQueuePresentKHR(demo->vulkan->present_queue, &present);
    demo->current_submission_index += 1;
    demo->current_submission_index %= FRAME_LAG;
    demo->first_swapchain_frame = false;

    if (err == VK_ERROR_SURFACE_LOST_KHR) {
        vkDestroySurfaceKHR(demo->vulkan->instance, demo->vulkan->surface, NULL);
        create_display_surface(demo->vulkan);
    } else {
        assert(!err);
    }
}

void demo_run_display(struct pwc_demo *demo) {
    while (!demo->demo_quit) {
        demo_draw(demo);
        demo->demo_current_frame++;

        if (demo->demo_frame_count != INT32_MAX && demo->demo_current_frame == demo->demo_frame_count) {
            demo->demo_quit = true;
        }
    }
}