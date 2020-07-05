project('LGhost', 'cpp',
  default_options: ['buildtype=release', 'b_ndebug=if-release', 'cpp_std=c++17'],
  meson_version: '>=0.48.0',
  version: '1'
)

sources = [
  'LGhost/LGhost.cpp',
  'LGhost/LGhost.h'
]

vapoursynth_dep = dependency('vapoursynth').partial_dependency(compile_args: true, includes: true)

libs = []

if host_machine.cpu_family().startswith('x86')
  add_project_arguments('-fno-math-errno', '-fno-trapping-math', '-DLGHOST_X86', '-mfpmath=sse', '-msse2', language: 'cpp')

  sources += [
    'LGhost/LGhost_SSE2.cpp',
    'LGhost/VCL2/instrset.h',
    'LGhost/VCL2/instrset_detect.cpp',
    'LGhost/VCL2/vector_convert.h',
    'LGhost/VCL2/vectorclass.h',
    'LGhost/VCL2/vectorf128.h',
    'LGhost/VCL2/vectorf256.h',
    'LGhost/VCL2/vectorf256e.h',
    'LGhost/VCL2/vectorf512.h',
    'LGhost/VCL2/vectorf512e.h',
    'LGhost/VCL2/vectori128.h',
    'LGhost/VCL2/vectori256.h',
    'LGhost/VCL2/vectori256e.h',
    'LGhost/VCL2/vectori512.h',
    'LGhost/VCL2/vectori512e.h',
    'LGhost/VCL2/vectori512s.h',
    'LGhost/VCL2/vectori512se.h',
    'LGhost/VCL2/vectormath_common.h',
    'LGhost/VCL2/vectormath_exp.h',
    'LGhost/VCL2/vectormath_hyp.h',
    'LGhost/VCL2/vectormath_lib.h',
    'LGhost/VCL2/vectormath_trig.h'
  ]

  libs += static_library('avx2', 'LGhost/LGhost_AVX2.cpp',
    dependencies: vapoursynth_dep,
    cpp_args: ['-mavx2', '-mfma'],
    gnu_symbol_visibility: 'hidden'
  )

  libs += static_library('avx512', 'LGhost/LGhost_AVX512.cpp',
    dependencies: vapoursynth_dep,
    cpp_args: ['-mavx512f', '-mavx512vl', '-mavx512bw', '-mavx512dq', '-mfma'],
    gnu_symbol_visibility: 'hidden'
  )
endif

shared_module('lghost', sources,
  dependencies: vapoursynth_dep,
  link_with: libs,
  install: true,
  install_dir: join_paths(vapoursynth_dep.get_pkgconfig_variable('libdir'), 'vapoursynth'),
  gnu_symbol_visibility: 'hidden'
)
