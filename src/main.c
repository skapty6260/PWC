#include <pwc/render/scene/node.h>
#include <pwc/render/render.h>
#include <pwc/render/scene/scene.h>
#include <pwc/render/vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wayland-version.h>

int main(int argc, char **argv) {
    printf("PURE WAYLAND COMPOSITOR\n");
    printf("WAYLAND version: %s\n", WAYLAND_VERSION);  // Use _S for string (if defined; fallback to manual)
    printf("PWC version: 0.01dev\n");

    struct pwc_vulkan *vulkan = calloc(1, sizeof(struct pwc_vulkan));
    if (!vulkan) {
        fprintf(stderr, "Failed to allocate vulkan\n");
        exit(EXIT_FAILURE);
        // return NULL;
    }
    vulkan->swapchain_ready = false;
    vulkan->initialized = false;
    vulkan->validate = true;
    
    init_vulkan(vulkan);
    printf("Vulkan initialized!\n");

    // struct pwc_render *render = create_render();
    // if (!render) {
    //     fprintf(stderr, "Failed to create render\n");
    //     exit(EXIT_FAILURE);
    // }

    // render_run(render);
    
    return EXIT_SUCCESS;
}