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

# Find the glslc compiler (or glslangvalidator)
# You might need to adjust the path or use find_program if it's not in PATH
glslc_compiler = find_program('glslc', required: true)

# Define the shader files to compile
shaders = ['include/pwc/render/shaders/shader.vert', 'include/pwc/render/shaders/shader.frag']

# Create a custom target for each shader
shader_targets = []
foreach shader : shaders
    shader_name = shader.split('/')[-1].split('.')[1] # Get name without extension
    spv_output = join_paths(meson.current_build_dir(), 'shaders', shader_name + '.spv')

    shader_target = custom_target(
        shader_name + '_spv',
        input: shader,
        output: shader_name + '.spv',
        command: [glslc_compiler, '@INPUT@', '-o', '@OUTPUT@'],
        install: true, # Install the compiled SPV files
        install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'my_project_name', 'shaders')
    )
    shader_targets += shader_target
endforeach

# If your executable needs to link against these shaders (e.g., embedding them)
# you can add shader_targets as a dependency to your executable.
# For Vulkan applications, you typically load the .spv files at runtime.
# If you are embedding them, you might need a custom step to generate a C++ header
# with the shader data.

inc_dir = include_directories('include')

subdir('include')
subdir('src')