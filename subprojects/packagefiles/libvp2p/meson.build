project('libp2p', 'cpp',
    default_options: ['buildtype=release', 'b_ndebug=if-release', 'cpp_std=c++14'],
    meson_version: '>=0.48.0'
)

p2p_args = []

if host_machine.cpu_family().startswith('x86')
    p2p_args += ['-DP2P_SIMD']
endif

libs = []

libs += static_library('p2p_main',
    [
        'p2p_api.cpp',
        'v210.cpp',
        'simd/cpuinfo_x86.cpp',
        'simd/p2p_simd.cpp'
    ],
    include_directories: ['simd'],
    cpp_args: p2p_args
)

if host_machine.cpu_family().startswith('x86')
    p2p_args += ['-msse4.1']

    libs += static_library('p2p_sse41',
        'simd/p2p_sse41.cpp',
        include_directories: ['simd'],
        cpp_args: p2p_args
    )
endif

libvp2p_dep = declare_dependency(link_with: libs, include_directories: include_directories('.'))
