project('BestSource', 'cpp',
    default_options: ['buildtype=release', 'b_ndebug=if-release', 'cpp_std=c++14'],
    meson_version: '>=0.48.0'
)

libs = []

sources = [
    'src/audiosource.cpp',
    'src/videosource.cpp',
    'src/SrcAttribCache.cpp',
    'src/BSRational.cpp',
]

libp2p_dep = dependency('libvp2p')
jansson_dep = dependency('jansson', version: '>= 2.7', required: true)

deps = [
    libp2p_dep,
    jansson_dep,
    dependency('libavcodec', version: '>=58.18.0'),
    dependency('libavformat', version: '>=58.12.0'),
    dependency('libavutil', version: '>=57.17.100'),
]

bs_lib = static_library('bestsource', sources,
    dependencies: deps,
    gnu_symbol_visibility: 'hidden'
)

bestsource_dep = declare_dependency(link_with: bs_lib, include_directories: include_directories('src'))
