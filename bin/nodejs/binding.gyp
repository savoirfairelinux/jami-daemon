{
  "targets": [
    {
      "target_name": "jamid",
      "sources": [ "jami_wrapper.cpp" ],
      'include_dirs': ['../../src/'],
      'libraries': ['-L<(module_root_dir)/../../src/.libs', '-ljami'],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions', '-fno-rtti', '-std=gnu++1y' ],
      'cflags_cc': [ '-std=gnu++17' ]
    }
  ]
}