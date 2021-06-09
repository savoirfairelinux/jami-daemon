nodejs_wrapper_target = custom_target('nodejs.wrapper',
    command: [progswig, '-v', '-c++', '-javascript', '-node', '-o', '@OUTPUT@', '@INPUT@'],
    input: 'nodejs_interface.i',
    output: 'nodejs_wrapper.cpp'
)

nodejs_makefile_target = custom_target('nodejs.makefile',
    command: [prognodegyp, 'configure', '--target=v14.16.1', '--arch=x64'],
    output: 'build/Makefile',
    depends: nodejs_wrapper_target
)

nodejs_node_target = custom_target('nodejs.node',
    command: [prognodegyp, 'build'],
    output: 'build/Release/obj.target/jamid.node',
    depends: nodejs_makefile_target,
    build_by_default: true
)
