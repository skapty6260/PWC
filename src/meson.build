sources = files(
    'main.c',
    'render/vulkan.c',
    'render/drm.c',
    'render/esTransform.c',
    'render/vulkan/vk-core.c',
    'render/vulkan/vk-debug.c',
    'render/demo.c',
)

wayland_server = dependency('wayland-server', version: '>=1.21.0')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols', version: '>=1.41', default_options: ['tests=false'])
drm = dependency('libdrm')
vulkan_dep = dependency('vulkan')
mathlib = cc.find_library('m', required: false)
gbm_dep = dependency('gbm')

deps = [
    wayland_client,
    wayland_cursor,
    wayland_protos,
    wayland_server,
    drm,
    gbm_dep,
    vulkan_dep,
    mathlib
]

executable(
    'pwc',
    sources,
    include_directories: [inc_dir],
    dependencies: deps,
    install: true,
    c_args: ['-std=c11', '-D_GNU_SOURCE', '-DWLR_USE_UNSTABLE', '-D_POSIX_C_SOURCE=200809L',
                    '-Wno-sign-compare', '-Wno-unused-function', '-Wno-error']
)
           