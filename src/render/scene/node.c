#include <pwc/render/scene/node.h>
#include <stdio.h>
#include <stdlib.h>

SceneNodeT *create_scene_node(enum SceneNodeType type, void *data) {
    SceneNodeT *node = calloc(1, sizeof(SceneNodeT));
    if (!node) {
        fprintf(stderr, "Failed to create scene node");
        return NULL;
    }

    node->type = type;
    node->data = data;
    node->next = NULL;
    node->prev = NULL;

    return node;
}

void scene_add_child(SceneNodeT *src, SceneNodeT *child) {
    if (!src || !child) return;
    child->prev = src;
    if (!src->next) {
        src->next = child;
    } else {
        printf("Src already have children!\n");
    }
}