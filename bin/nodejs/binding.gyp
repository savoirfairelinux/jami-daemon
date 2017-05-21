{
  "targets": [
    {
      "target_name": "dring",
      "sources": [ "ring_wrapper.cpp" ],
      'include_dirs': ['../../src/'],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ]
    }
  ]
}