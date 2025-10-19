#include <pwc/render/scene/node.h>
#include "pwc/render/scene/scene.h"
#include <pwc/render/vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wayland-version.h>

int main(int argc, char **argv) {
    printf("PURE WAYLAND COMPOSITOR\n");
    printf("WAYLAND version: %s\n", WAYLAND_VERSION);  // Use _S for string (if defined; fallback to manual)
    printf("PWC version: 0.01dev\n");

    struct pwc_scene *scene = create_scene();
    if (!scene) {
        fprintf(stderr, "Failed to create scene\n");
        return EXIT_FAILURE;
    }

    SceneNodeT *root = create_scene_node(SCENE_NODE_ROOT, NULL);
    scene->root = root;

    SceneNodeT *ws1 = create_scene_node(SCENE_NODE_WORKSPACE, NULL);
    scene_add_child(root, ws1);

    SceneNodeT *bg1 = create_scene_node(SCENE_NODE_BACKGROUND, NULL);
    scene_add_child(ws1, bg1);

    SceneNodeT *ws2 = create_scene_node(SCENE_NODE_WORKSPACE, NULL);
    scene_add_child(root, ws2);

    SceneNodeT *bg2 = create_scene_node(SCENE_NODE_BACKGROUND, NULL);
    scene_add_child(ws2, bg2);

    print_scene(scene);
    destroy_scene(scene);

    struct pwc_vulkan *vulkan = calloc(1, sizeof(struct pwc_vulkan));
    vulkan->validate = true;
    init_vulkan(vulkan);

    free(vulkan);

    return EXIT_SUCCESS;
}