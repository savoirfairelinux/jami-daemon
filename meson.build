project('jami-daemon', ['c', 'cpp'],
        version: '9.9.0',
        license: 'GPL3+',
        default_options: ['cpp_std=gnu++17', 'buildtype=debugoptimized'],
        meson_version:'>= 0.54'
       )

modpkgconfig = import('pkgconfig')
modcmake = import('cmake')

#################################################
# Required dependencies
#################################################
depthreads = dependency('threads')
depopendht = dependency('opendht', version: '>= 2.1.0')
depgnutls = dependency('gnutls', version: '>= 3.6.7')
deplibpjproject = dependency('libpjproject')
deplibgit2 = dependency('libgit2', version: '>= 1.1.0')
deplibsecp256k1 = dependency('libsecp256k1', version: '>= 0.1')
deplibavcodec = dependency('libavcodec', version: '>= 56.60.100')
deplibavfilter = dependency('libavfilter', version: '>= 5.40.101')
deplibavdevice = dependency('libavdevice', version: '>= 56.4.100')
deplibavformat = dependency('libavformat', version: '>= 56.40.101')
deplibswscale = dependency('libswscale', version: '>= 3.1.101')
deplibswresample = dependency('libswresample', version: '>= 1.2.101')
deplibavutil = dependency('libavutil', version: '>= 55.75.100')
depspeexdsp = dependency('speexdsp')
depfmt = dependency('fmt', version: '>= 5.3')

depyamlcpp = dependency('yaml-cpp', version: '>= 0.5.1', required: false)
if not depyamlcpp.found()
    depyamlcpp = modcmake.subproject('yaml-cpp', cmake_options: [
        '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DYAML_CPP_BUILD_TOOLS=OFF',
        '-DYAML_CPP_BUILD_TESTS=OFF',
        '-DYAML_CPP_BUILD_CONTRIB=OFF',
        '-DBUILD_SHARED_LIBS=OFF'
    ]).dependency('yaml-cpp')
endif

depjsoncpp = dependency('jsoncpp', version: '>= 1.6.5',
                        fallback: ['jsoncpp', 'jsoncpp_dep'], default_options: ['default_library=static'])
depzlib = dependency('zlib', fallback: ['zlib', 'zlib_dep'], default_options: ['default_library=static'])

if get_option('interfaces').contains('dbus')
    depdbuscpp = dependency('dbus-c++-1')
    progdbusxxxml2cpp = find_program('dbusxx-xml2cpp', native: true)
endif

if get_option('interfaces').contains('nodejs')
    progswig = find_program('swig', native: true)
    prognodegyp = find_program('node-gyp', native: true)
endif

if get_option('tests')
    depcppunit = dependency('cppunit', version: '>= 1.12')
endif

#################################################
# Optional dependencies and configuration
#################################################
progpod2man = find_program('pod2man', native: true, required: false)

conf = configuration_data()
conf.set_quoted('PACKAGE', 'jami')
conf.set_quoted('PACKAGE_NAME', 'Jami Daemon')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_STRING', 'Jami Daemon ' + meson.project_version())
conf.set_quoted('PROGSHAREDIR', get_option('datadir') / 'ring')

depopensl = meson.get_compiler('cpp').find_library('OpenSLES', has_headers: 'SLES/OpenSLES.h', required: get_option('opensl'))
conf.set10('HAVE_OPENSL', depopensl.found())

depalsa = dependency('alsa', version: '>= 1.0', required: get_option('alsa'))
conf.set10('HAVE_ALSA', depalsa.found())

deplibpulse = dependency('libpulse', version: '>= 0.9.15', required: get_option('pulseaudio'))
conf.set10('HAVE_PULSE', deplibpulse.found())

depjack = dependency('jack', required: get_option('jack'))
conf.set10('HAVE_JACK', depjack.found())

depportaudio = dependency('portaudio-2.0', required: get_option('portaudio'))
conf.set10('HAVE_PORTAUDIO', depportaudio.found())

deplibupnp = dependency('libupnp', required: get_option('upnp'))
conf.set10('HAVE_LIBUPNP', deplibupnp.found())

if get_option('natpmp_prefix') == ''
    depnatpmp = meson.get_compiler('cpp').find_library('natpmp', has_headers: 'natpmp.h', required: get_option('natpmp'))
else
    depnatpmp_ = meson.get_compiler('cpp').find_library('natpmp', dirs: get_option('natpmp_prefix') / 'lib', required: get_option('natpmp'))
    if depnatpmp_.found()
        depnatpmp = declare_dependency(
            compile_args: '-I' + get_option('natpmp_prefix') / 'include',
            dependencies: depnatpmp_
        )
    else
        depnatpmp = depnatpmp_
    endif
endif
conf.set10('HAVE_LIBNATPMP', depnatpmp.found())

depopenssl = dependency('openssl', required: get_option('name_service'))
conf.set10('HAVE_RINGNS', depopenssl.found())

depwebrtcap = dependency('webrtc-audio-processing', required: get_option('aec'))
conf.set10('HAVE_WEBRTC_AP', depwebrtcap.found())

if get_option('video')
    conf.set('ENABLE_VIDEO', true)
    if host_machine.system() == 'linux' and meson.get_compiler('cpp').get_define('__ANDROID__') != '1'
        deplibudev = dependency('libudev')
    endif

    if get_option('hw_acceleration')
        conf.set('RING_ACCEL', true)
        conf.set('ENABLE_VIDEOTOOLBOX', host_machine.system() == 'darwin')
    else
        conf.set('RING_ACCEL', false)
        conf.set('ENABLE_VIDEOTOOLBOX', false)
    endif
else
    conf.set('ENABLE_VIDEO', false)
    conf.set('RING_ACCEL', false)
    conf.set('ENABLE_VIDEOTOOLBOX', false)
endif

if get_option('plugins')
    conf.set('ENABLE_PLUGIN', true)
    deplibarchive = dependency('libarchive', version: '>= 3.4.0', required: false)
    if not deplibarchive.found()
        deplibarchive = modcmake.subproject('libarchive', cmake_options: [
            '-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
            '-DENABLE_TEST=OFF',
            '-DENABLE_TAR=OFF',
            '-DENABLE_CPIO=OFF',
            '-DENABLE_CAT=OFF',
            '-DENABLE_LIBXML2=OFF'
        ]).dependency('archive_static')
    endif
    depdl = meson.get_compiler('cpp').find_library('dl', required: false)
else
    conf.set('ENABLE_PLUGIN', false)
endif

conf.set10('HAVE_COREAUDIO', host_machine.system() == 'darwin')
conf.set10('HAVE_SHM', host_machine.system() == 'linux' and meson.get_compiler('cpp').get_define('__ANDROID__') != '1')

configure_file(
    output: 'config.h',
    configuration: conf
)
add_project_arguments('-DHAVE_CONFIG_H', language: 'cpp')
add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

#################################################
# Build targets
#################################################
add_project_arguments('-DRING_BUILD', language: 'cpp')
add_project_arguments('-DRING_BUILD', language: 'c')
if get_option('default_library') != 'static'
    add_project_arguments('-Ddring_EXPORTS', language: 'cpp')
    add_project_arguments('-Ddring_EXPORTS', language: 'c')
endif

add_project_arguments('-DASIO_STANDALONE', language: 'cpp')
add_project_arguments('-DASIO_STANDALONE', language: 'c')

subdir('src')

if get_option('interfaces').contains('dbus')
    subdir('bin' / 'dbus')
endif

if get_option('interfaces').contains('nodejs')
    subdir('bin' / 'nodejs')
endif

if get_option('tests')
    subdir('test')
endif

#################################################
# Resources and metafiles
#################################################
subdir('ringtones')

if host_machine.system() == 'linux' and meson.get_compiler('cpp').get_define('__ANDROID__') != '1'
    provides_content = ''
    if get_option('interfaces').contains('dbus')
        provides_content += '  <dbus type="user">cx.ring.Ring</dbus>'
    endif
    if get_option('interfaces').contains('library')
        provides_content += (provides_content == '')? '' : '\n'
        if get_option('default_library') == 'static'
            provides_content += '  <library>libjami.a</library>'
        elif get_option('default_library') == 'shared'
            provides_content += '  <library>libjami.so</library>'
        else
            provides_content += '  <library>libjami.so</library>\n'
            provides_content += '  <library>libjami.a</library>'
        endif
    endif
    if provides_content != ''
        configure_file(
            configuration: {'PROVIDES_CONTENT': provides_content},
            input: 'net.jami.daemon.metainfo.xml.in',
            output: 'net.jami.daemon.metainfo.xml',
            install: true,
            install_dir: get_option('datadir') / 'metainfo'
        )
    endif
endif
