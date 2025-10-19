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
    node->parent = NULL;
    node->num_child = 0;
    node->capacity = 0;
    node->child = NULL;

    return node;
}

void scene_add_child(SceneNodeT *src, SceneNodeT *child) {
    if (!src || !child) return;
    
    if (src->num_child >= src->capacity) {
        int new_capacity = (src->capacity == 0) ? 10 : src->capacity * 2;
        SceneNodeT **new_array = realloc(src->child, new_capacity * sizeof(SceneNodeT *));
        if (!new_array) {
            fprintf(stderr, "Failed to realloc children array\n");
            return;
        }

        src->child = new_array;
        src->capacity = new_capacity; 
    }

    src->child[src->num_child] = child;
    src->num_child++;
    child->parent = src;
}