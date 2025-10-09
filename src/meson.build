sources = files(
    'main.c',
)

wayland_server = dependency('wayland-server', version: '>=1.21.0')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols', version: '>=1.41', default_options: ['tests=false'])
drm = dependency('libdrm')
gbm_dep = dependency('gbm')
egl_dep = dependency('egl')
glesv2_dep = dependency('glesv2')
mathlib = cc.find_library('m', required: false)

deps = [
    wayland_client,
    wayland_cursor,
    wayland_protos,
    wayland_server,
    drm,
    egl_dep,
    glesv2_dep,
    gbm_dep,
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
           