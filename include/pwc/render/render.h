#ifndef _PWC_RENDER_H
#define _PWC_RENDER_H

#include <pwc/render/vulkan/vulkan.h>

struct pwc_render {
    struct pwc_vulkan *vulkan;

};

// Рендер должен иметь в себе имплементацию вулкана, структуру сцены и методы работы со сценой.

#endif