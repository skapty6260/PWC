project(
    'pwc',
    'c',
    version: '0.01-dev',
    license: 'MIT',
    meson_version: '>=1.3',
    default_options: [
        'c_std=c11',
        'warning_level=2',
        'werror=true'
    ]
)

add_project_arguments(
    [
		'-DWLR_USE_UNSTABLE',
		'-D_POSIX_C_SOURCE=200809L',

		'-Wno-unused-parameter',
		'-Wno-unused-result',
		'-Wno-unused-but-set-variable',
		'-Wno-missing-braces',
		'-Wno-format-zero-length',
		'-Wundef',
		'-Wvla',
	],
	language: 'c',
)

cc = meson.get_compiler('c')

is_freebsd = host_machine.system().startswith('freebsd')

if is_freebsd
	add_project_arguments('-D_C11_SOURCE', language: 'c')
endif

wayland_server = dependency('wayland-server', version: '>=1.21.0')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols', version: '>=1.41', default_options: ['tests=false'])

inc_dir = include_directories('include')

subdir('include')
subdir('src')