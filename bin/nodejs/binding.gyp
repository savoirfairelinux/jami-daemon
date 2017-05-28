{
  "targets": [
    {
      "target_name": "dring",
      "sources": [ "ring_wrapper.cpp" ],
      'include_dirs': ['../../src/'],
      'libraries': ['-L<(module_root_dir)/../../src/.libs/', '-lring'],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ]
    }
  ]
}