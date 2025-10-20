#ifndef _PWC_RENDER_SCENE_NODE_H
#define _PWC_RENDER_SCENE_NODE_H

#include <stdbool.h>
#include <vulkan/vulkan_core.h>
enum SceneNodeType {
    SCENE_NODE_ROOT = 1,
    SCENE_NODE_WORKSPACE = 2,
    SCENE_NODE_BACKGROUND = 3,
    SCENE_NODE_CONTAINER = 4,
    SCENE_NODE_UNKNOWN = 5
};

typedef struct NodeRenderData {
    VkPipeline pipeline;        // Graphics pipeline for this node type
    VkBuffer vertex_buffer;     // Vertex data (e.g., quad for background)
    VkDeviceMemory vertex_mem;
    VkBuffer index_buffer;      // Optional: for indexed drawing
    VkDeviceMemory index_mem;
    VkDescriptorSet descriptor_set;  // For uniforms/textures
    // Add more: e.g., VkImage for textures, push constants for transforms
} NodeRenderData;

typedef struct SceneNode {
    enum SceneNodeType type;

    void *data;

    bool is_dirty;

    struct SceneNode **child;
    int num_child;
    int capacity;
    struct SceneNode *parent;
} SceneNodeT;

SceneNodeT *create_scene_node(enum SceneNodeType type, void *data);
void destroy_scene_node(SceneNodeT *node);
void scene_add_child(SceneNodeT *src, SceneNodeT *child);

#endif