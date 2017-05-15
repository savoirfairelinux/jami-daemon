{
  "targets": [
    {
      "target_name": "dring",
      "sources": [ "ring_wrapper.cpp" ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ]
    }
  ]
}