#ifndef _PWC_RENDER_SCENE_NODE_H
#define _PWC_RENDER_SCENE_NODE_H

enum SceneNodeType {
    SCENE_NODE_ROOT = 1,
    SCENE_NODE_WORKSPACE = 2,
    SCENE_NODE_BACKGROUND = 3,
    SCENE_NODE_CONTAINER = 4,
    SCENE_NODE_UNKNOWN = 5
};

typedef struct SceneNode {
    enum SceneNodeType type;

    void *data;

    struct SceneNode **child;
    int num_child;
    int capacity;
    struct SceneNode *parent;
} SceneNodeT;

SceneNodeT *create_scene_node(enum SceneNodeType type, void *data);
void destroy_scene_node(SceneNodeT *node);
void scene_add_child(SceneNodeT *src, SceneNodeT *child);

#endif