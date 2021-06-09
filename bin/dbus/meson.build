jamid_includedirs = include_directories(
    '../..',
    '../../src',
    '../../src/jamid'
)

jamid_sources = files(
    'dbuscallmanager.cpp',
    'dbusclient.cpp',
    'dbusconfigurationmanager.cpp',
    'dbusinstance.cpp',
    'dbuspresencemanager.cpp',
    'main.cpp'
)

jamid_targets = []
jamid_targets += custom_target('callmanager.adaptor',
    command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
    input: 'cx.ring.Ring.CallManager.xml',
    output: 'dbuscallmanager.adaptor.h'
)

jamid_targets += custom_target('configurationmanager.adaptor',
    command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
    input: 'cx.ring.Ring.ConfigurationManager.xml',
    output: 'dbusconfigurationmanager.adaptor.h'
)

jamid_targets += custom_target('instance.adaptor',
    command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
    input: 'cx.ring.Ring.Instance.xml',
    output: 'dbusinstance.adaptor.h'
)

jamid_targets += custom_target('presencemanager.adaptor',
    command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
    input: 'cx.ring.Ring.PresenceManager.xml',
    output: 'dbuspresencemanager.adaptor.h'
)

if conf.get('ENABLE_VIDEO')
    jamid_targets += custom_target('videomanager.adaptor',
        command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
        input: 'cx.ring.Ring.VideoManager.xml',
        output: 'dbusvideomanager.adaptor.h'
    )

    jamid_sources += files(
        'dbusvideomanager.cpp'
    )
endif

if conf.get('ENABLE_PLUGIN')
    jamid_targets += custom_target('pluginmanagerinterface.adaptor',
        command: [progdbusxxxml2cpp, '@INPUT@', '--adaptor=@OUTPUT@'],
        input: 'cx.ring.Ring.PluginManagerInterface.xml',
        output: 'dbuspluginmanagerinterface.adaptor.h'
    )

    jamid_sources += files(
        'dbuspluginmanagerinterface.cpp'
    )
endif

if progpod2man.found()
    jamid_targets += custom_target('jamid.man',
        command: [progpod2man, '--section=8', '--release='+meson.project_version(), '--center', '""', '@INPUT@', '@OUTPUT@'],
        input: 'jamid.pod',
        output: 'jamid.8',
        install: true,
        install_dir: get_option('mandir') / 'man8'
    )
endif

jamid = executable('jamid',
    jamid_targets,
    include_directories: jamid_includedirs,
    sources: jamid_sources,
    dependencies: depdbuscpp,
    link_with: libjami,
    install: true,
    install_dir: get_option('libdir')
)

configure_file(
    configuration: {'LIBDIR': get_option('prefix') / get_option('libdir')},
    input: 'net.jami.daemon.service.in',
    output: 'cx.ring.Ring.service',
    install: true,
    install_dir: get_option('datadir') / 'dbus-1' / 'services'
)

install_data(
    'cx.ring.Ring.CallManager.xml',
    'cx.ring.Ring.ConfigurationManager.xml',
    'cx.ring.Ring.Instance.xml',
    'cx.ring.Ring.PresenceManager.xml',
    install_dir: get_option('datadir') / 'dbus-1' / 'interfaces'
)
if conf.get('ENABLE_VIDEO')
    install_data(
        'cx.ring.Ring.VideoManager.xml',
        install_dir: get_option('datadir') / 'dbus-1' / 'interfaces'
    )
endif
if conf.get('ENABLE_PLUGIN')
    install_data(
        'cx.ring.Ring.PluginManagerInterface.xml',
        install_dir: get_option('datadir') / 'dbus-1' / 'interfaces'
    )
endif
