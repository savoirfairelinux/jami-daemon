nodejs_wrapper_target = custom_target('nodejs.wrapper',
    command: [progswig, '-v', '-c++', '-javascript', '-napi', '-o', '@OUTPUT@', '@INPUT@'],
    input: 'nodejs_interface.i',
    output: 'nodejs_wrapper.cpp'
)

nodejs_makefile_target = custom_target('nodejs.makefile',
    command: ['sh', '-c',
              prognodegyp.full_path() + ' configure --target=v14.16.1 --arch=x64 && touch @OUTPUT@'],
    output: 'nodejs_makefile.stamp',
    depends: nodejs_wrapper_target
)

nodejs_node_target = custom_target('nodejs.node',
    command: ['sh', '-c',
              prognodegyp.full_path() + ' build && touch @OUTPUT@'],
    output: 'nodejs_node.stamp',
    depends: nodejs_makefile_target,
    build_by_default: true
)
