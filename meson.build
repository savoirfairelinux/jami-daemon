project('jami-daemon', 'cpp',
        version: '7.2.0',
        license: 'GPL3+',
        default_options: ['cpp_std=gnu++14'],
        meson_version:'>= 0.49'
       )

depopendht = dependency('opendht')
depgnutls = dependency('gnutls')
depyaml-cpp = dependency('yaml-cpp')
depjsoncpp = dependency('jsoncpp')
deplibsecp256k1 = dependency('libsecp256k1')
deplibavcodec = dependency('libavcodec')
deplibavfilter = dependency('libavfilter')
deplibavdevice = dependency('libavdevice')
deplibavformat = dependency('libavformat')
deplibswscale = dependency('libswscale')
deplibswresample = dependency('libswresample')
deplibavutil = dependency('libavutil')
deppjproject = dependency('pjproject') !!!

if get_option('interfaces').contains('dbus')
    depdbus-c++ = dependency('dbus-c++')
endif

if get_option('interfaces').contains('rest')
    deprestbed = dependency('restbed')
endif

depopensl = dependency('opensl', required: get_option('opensl'))
depalsa = dependency('alsa', required: get_option('alsa'))
deplibpulse = dependency('libpulse', required: get_option('pulseaudio'))
depjack = dependency('jack', required: get_option('jack'))
depportaudio = dependency('portaudio', required: get_option('portaudio'))
depopus = dependency('opus', required: get_option('opus'))
deplibupnp = dependency('libupnp', required: get_option('upnp'))
deplibnatpmp = dependency('libnatpmp', required: get_option('natpmp'))
deplibpcre = dependency('libpcre', required: get_option('sdes'))

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', 'Jami Daemon')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_STRING', 'Jami Daemon '+meson.project_version())
conf.set10('HAVE_IPV6', get_option('ipv6'))
conf.set10('HAVE_RINGNS', get_option('name_service'))

if get_option('video')
    if host_machine.system() == 'linux' and not meson.get_compiler('cpp').get_define('__ANDROID__') == '1'
        deplibudev = dependency('libudev')
    endif
    conf.set('RING_VIDEO', true)
endif

if get_option('hw_acceleration')
    conf.set('RING_ACCEL', true)

    deplibva = dependency('libva', required: false)
    if deplibva.found()
        deplibva-drm = dependency('libva-drm', required: false)
        conf.set10('HAVE_VAAPI_ACCEL_DRM', deplibva-drm.found())
        deplibva-x11 = dependency('libva-x11', required: false)
        conf.set10('HAVE_VAAPI_ACCEL_x11', deplibva-x11.found())
    endif

endif

configure_file(
    output: 'config.h',
    configuration: conf
)

add_project_arguments('-DHAVE_CONFIG_H "-I'+meson.current_build_dir()+'"', language: 'cpp')
add_project_arguments('-DPROGSHAREDIR="'+get_option('datadir')+'/ring"', language: 'cpp')

subdir('src')
