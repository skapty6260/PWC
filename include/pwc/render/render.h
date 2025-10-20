#ifndef _PWC_RENDER_H
#define _PWC_RENDER_H

#include <pwc/render/scene/scene.h>
#include <pwc/render/vulkan/vulkan.h>

struct pwc_render {
    struct pwc_vulkan *vulkan;
    struct pwc_scene *scene;

    bool running;
};

// Рендер должен иметь в себе имплементацию вулкана, структуру сцены и методы работы со сценой.

struct pwc_render *create_render(void);
void render_run(struct pwc_render *render);
void render_destroy(struct pwc_render *render);

#endif