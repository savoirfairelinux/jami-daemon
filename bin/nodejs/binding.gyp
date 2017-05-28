{
  "targets": [
    {
      "target_name": "dring",
      "sources": [ "ring_wrapper.cpp" ],
      'include_dirs': ['../../src/'],
      'libraries': [#linking libring
      				'-L<(module_root_dir)/../../src/.libs/',
      				'-lring',
      				#linking dependencies
      				'-L<(module_root_dir)/../../contrib/x86_64-linux-gnu/lib',
      				'-lnettle -lhogweed'],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ]
    }
  ]
}