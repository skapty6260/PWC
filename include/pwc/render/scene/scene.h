#ifndef _PWC_RENDER_SCENE_H
#define _PWC_RENDER_SCENE_H

// Это должно быть дерево. Структура такая: root (have cursor and etc., not rendering by itself)->workspaces->background->widgets
//                                                                                              ->containers (windows)

#include <stdint.h>
#include <pwc/render/scene/node.h>

struct pwc_scene {
    SceneNodeT *root;
};

void print_scene(struct pwc_scene *scene);
struct pwc_scene *create_scene(void);

#endif