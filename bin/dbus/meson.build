jamid_includedirs = include_directories(
    '../..',
    '../../src',
    '../../src/dring',
    '../../src/media'
)

jamid_sources = files(
    'dbuscallmanager.cpp',
    'dbusclient.cpp',
    'dbusconfigurationmanager.cpp',
    'dbusinstance.cpp',
    'dbuspresencemanager.cpp',
    'main.cpp'
)

adaptors = []
adaptors += custom_target('callmanager.adaptor',
    command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
    input: 'cx.ring.Ring.CallManager.xml',
    output: 'dbuscallmanager.adaptor.h'
)

adaptors += custom_target('configurationmanager.adaptor',
    command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
    input: 'cx.ring.Ring.ConfigurationManager.xml',
    output: 'dbusconfigurationmanager.adaptor.h'
)

adaptors += custom_target('instance.adaptor',
    command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
    input: 'cx.ring.Ring.Instance.xml',
    output: 'dbusinstance.adaptor.h'
)

adaptors += custom_target('presencemanager.adaptor',
    command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
    input: 'cx.ring.Ring.PresenceManager.xml',
    output: 'dbuspresencemanager.adaptor.h'
)

if conf.get('ENABLE_VIDEO')
    adaptors += custom_target('videomanager.adaptor',
        command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
        input: 'cx.ring.Ring.VideoManager.xml',
        output: 'dbusvideomanager.adaptor.h'
    )

    jamid_sources += files(
        'dbusvideomanager.cpp'
    )
endif

jamid = executable('jamid',
    adaptors,
    include_directories: jamid_includedirs,
    sources: jamid_sources,
    dependencies: depdbuscpp,
    link_with: libjami,
    install: true,
    install_dir: get_option('prefix') / get_option('libdir')
)

configure_file(
    configuration: {'LIBDIR': get_option('prefix') / get_option('libdir')},
    input: 'net.jami.daemon1.service.in',
    output: 'cx.ring.Ring.service',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'dbus-1' / 'services'
)
