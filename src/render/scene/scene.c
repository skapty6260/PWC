#include <pwc/render/scene/node.h>
#include <pwc/render/scene/scene.h>
#include <stdio.h>
#include <stdlib.h>

struct pwc_scene *create_scene(void) {
    struct pwc_scene *scene = calloc(1, sizeof(struct pwc_scene));
    if (!scene) {
        fprintf(stderr, "Failed to allocate scene\n");
        return NULL;
    }
    scene->root = NULL;
    return scene;
}

static void free_node_rec(SceneNodeT *node) {
    if (!node) return;
    for (int i = 0; i < node->num_child; i++) {
        free_node_rec(node->child[i]);
    }
    free(node->child);
    free(node->data);
    free(node);
}

void destroy_scene(struct pwc_scene *scene) {
    if (!scene) return;
    
    free_node_rec(scene->root);
    free(scene);
}

static void print_node(SceneNodeT *node, int depth) {
        if (!node) return;
        // Indent based on depth
        for (int i = 0; i < depth; i++) printf("  ");
        const char *nodetype = "";
        switch (node->type) {
            case SCENE_NODE_ROOT:
                nodetype = "ROOT";
                break;
            case SCENE_NODE_WORKSPACE:
                nodetype = "WORKSPACE";
                break;
            case SCENE_NODE_BACKGROUND:
                nodetype = "BACKGROUND";
                break;
            case SCENE_NODE_CONTAINER:
                nodetype = "CONTAINER";
                break;
            default:
                nodetype = "UNKNOWN";
                break;
        }
        printf("Node type: %s, Data: %s\n", nodetype, (char *)node->data);

        // Print children from array
        for (int i = 0; i < node->num_child; i++) {
            print_node(node->child[i], depth + 1);
        }
}

void print_scene(struct pwc_scene *scene) {
    if (!scene || !scene->root) {
        printf("Scene is empty\n");
        return;
    }
    
    print_node(scene->root, 0);
}