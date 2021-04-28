libjami_includedirs = include_directories(
    '..',
    'dring',
    'media',
    'jamidht/eth'
)

libjami_sources = files(
    'client/callmanager.cpp',
    'client/configurationmanager.cpp',
    'client/conversation_interface.cpp',
    'client/datatransfer.cpp',
    'client/presencemanager.cpp',
    'client/ring_signal.cpp',
    'config/yamlparser.cpp',
    'im/instant_messaging.cpp',
    'im/message_engine.cpp',
    'jamidht/eth/libdevcore/Common.cpp',
    'jamidht/eth/libdevcore/CommonData.cpp',
    'jamidht/eth/libdevcore/FixedHash.cpp',
    'jamidht/eth/libdevcore/SHA3.cpp',
    'jamidht/eth/libdevcrypto/Common.cpp',
    'jamidht/accountarchive.cpp',
    'jamidht/account_manager.cpp',
    'jamidht/archive_account_manager.cpp',
    'jamidht/channeled_transfers.cpp',
    'jamidht/channeled_transport.cpp',
    'jamidht/connectionmanager.cpp',
    'jamidht/contact_list.cpp',
    'jamidht/conversation.cpp',
    'jamidht/conversationrepository.cpp',
    'jamidht/gitserver.cpp',
    'jamidht/jamiaccount.cpp',
    'jamidht/multiplexed_socket.cpp',
    'jamidht/namedirectory.cpp',
    'jamidht/p2p.cpp',
    'jamidht/server_account_manager.cpp',
    'jamidht/sips_transport_ice.cpp',
    'media/audio/echo-cancel/null_echo_canceller.cpp',
    'media/audio/sound/audiofile.cpp',
    'media/audio/sound/dtmf.cpp',
    'media/audio/sound/dtmfgenerator.cpp',
    'media/audio/sound/tone.cpp',
    'media/audio/sound/tonelist.cpp',
    'media/audio/audiobuffer.cpp',
    'media/audio/audio_frame_resizer.cpp',
    'media/audio/audio_input.cpp',
    'media/audio/audiolayer.cpp',
    'media/audio/audioloop.cpp',
    'media/audio/audio_receive_thread.cpp',
    'media/audio/audio_rtp_session.cpp',
    'media/audio/audio_sender.cpp',
    'media/audio/dcblocker.cpp',
    'media/audio/dsp.cpp',
    'media/audio/resampler.cpp',
    'media/audio/ringbuffer.cpp',
    'media/audio/ringbufferpool.cpp',
    'media/audio/tonecontrol.cpp',
    'media/congestion_control.cpp',
    'media/libav_utils.cpp',
    'media/localrecorder.cpp',
    'media/localrecordermanager.cpp',
    'media/media_attribute.cpp',
    'media/media_buffer.cpp',
    'media/media_codec.cpp',
    'media/media_decoder.cpp',
    'media/media_encoder.cpp',
    'media/media_filter.cpp',
    'media/media_io_handle.cpp',
    'media/media_player.cpp',
    'media/media_recorder.cpp',
    'media/recordable.cpp',
    'media/socket_pair.cpp',
    'media/srtp.c',
    'media/system_codec_container.cpp',
    'security/certstore.cpp',
    'security/diffie-hellman.cpp',
    'security/memory.cpp',
    'security/tls_session.cpp',
    'security/tlsvalidator.cpp',
    'sip/pres_sub_client.cpp',
    'sip/pres_sub_server.cpp',
    'sip/sdes_negotiator.cpp',
    'sip/sdp.cpp',
    'sip/sipaccountbase.cpp',
    'sip/sipaccount.cpp',
    'sip/sipcall.cpp',
    'sip/sippresence.cpp',
    'sip/siptransport.cpp',
    'sip/sip_utils.cpp',
    'sip/sipvoiplink.cpp',
    'upnp/protocol/igd.cpp',
    'upnp/protocol/mapping.cpp',
    'upnp/upnp_context.cpp',
    'upnp/upnp_control.cpp',
    'account.cpp',
    'account_factory.cpp',
    'archiver.cpp',
    'base64.cpp',
    'buildinfo.cpp',
    'call.cpp',
    'call_factory.cpp',
    'conference.cpp',
    'data_transfer.cpp',
    'fileutils.cpp',
    'ftp_server.cpp',
    'gittransport.cpp',
    'ice_transport.cpp',
    'ip_utils.cpp',
    'logger.cpp',
    'manager.cpp',
    'peer_connection.cpp',
    'preferences.cpp',
    'ring_api.cpp',
    'scheduled_executor.cpp',
    'smartools.cpp',
    'string_utils.cpp',
    'threadloop.cpp',
    'utf8_utils.cpp'
)
if host_machine.system() == 'windows'
    libjami_sources += files('winsyslog.c')
endif

libjami_dependencies = [
    depthreads,
    depopendht,
    depgnutls,
    deplibpjproject,
    deplibgit2,
    deplibsecp256k1,
    deplibavcodec,
    deplibavfilter,
    deplibavdevice,
    deplibavformat,
    deplibswscale,
    deplibswresample,
    deplibavutil,
    depspeexdsp,
    depfmt,
    depyamlcpp,
    depjsoncpp,
    depzlib
]

if conf.get('HAVE_OPENSL') == 1
    libjami_sources += files(
        'media/audio/opensl/audio_player.cpp',
        'media/audio/opensl/audio_recorder.cpp',
        'media/audio/opensl/opensllayer.cpp'
    )
    libjami_dependencies += depopensl
endif

if conf.get('HAVE_ALSA') == 1
    libjami_sources += files(
        'media/audio/alsa/alsalayer.cpp'
    )
    libjami_dependencies += depalsa
endif

if conf.get('HAVE_PULSE') == 1
    libjami_sources += files(
        'media/audio/pulseaudio/audiostream.cpp',
        'media/audio/pulseaudio/pulselayer.cpp'
    )
    libjami_dependencies += deplibpulse
endif

if conf.get('HAVE_JACK') == 1
    libjami_sources += files(
        'media/audio/jack/jacklayer.cpp'
    )
    libjami_dependencies += depjack
endif

if conf.get('HAVE_PORTAUDIO') == 1
    libjami_sources += files(
        'media/audio/portaudio/portaudiolayer.cpp'
    )
    libjami_dependencies += depportaudio
endif

if conf.get('HAVE_COREAUDIO') == 1
    if meson.get_compiler('cpp').compiles('''#import <TargetConditionals.h>
                                             #if TARGET_OS_IPHONE == 0
                                             #error This is not an IPHONE
                                             #endif''')
        libjami_sources += files(
            'media/audio/coreaudio/ios/corelayer.mm'
        )
    else
        libjami_sources += files(
            'media/audio/coreaudio/osx/audiodevice.cpp',
            'media/audio/coreaudio/osx/corelayer.cpp'
        )
    endif
endif

if conf.get('HAVE_LIBUPNP') == 1
    libjami_sources += files(
        'upnp/protocol/pupnp/pupnp.cpp',
        'upnp/protocol/pupnp/upnp_igd.cpp'
    )
    libjami_dependencies += deplibupnp
endif

if conf.get('HAVE_LIBNATPMP') == 1
    libjami_sources += files(
        'upnp/protocol/natpmp/nat_pmp.cpp',
        'upnp/protocol/natpmp/pmp_igd.cpp'
    )
    libjami_dependencies += depnatpmp
endif

if conf.get('HAVE_RINGNS') == 1
    libjami_dependencies += depopenssl
endif

if conf.get('HAVE_WEBRTC_AP') == 1
    libjami_sources += files(
        'media/audio/echo-cancel/webrtc_echo_canceller.cpp'
    )
    libjami_dependencies += depwebrtcap
endif

if conf.get('ENABLE_VIDEO')
    libjami_sources += files(
        'client/videomanager.cpp',
        'media/video/filter_transpose.cpp',
        'media/video/sinkclient.cpp',
        'media/video/video_base.cpp',
        'media/video/video_device_monitor.cpp',
        'media/video/video_input.cpp',
        'media/video/video_mixer.cpp',
        'media/video/video_receive_thread.cpp',
        'media/video/video_rtp_session.cpp',
        'media/video/video_scaler.cpp',
        'media/video/video_sender.cpp'
    )

    if conf.get('RING_ACCEL')
        libjami_sources += files(
            'media/video/accel.cpp'
        )
    endif

    if host_machine.system() == 'linux'
        if meson.get_compiler('cpp').get_define('__ANDROID__') == '1'
            libjami_sources += files(
                'media/video/androidvideo/video_device_impl.cpp',
                'media/video/androidvideo/video_device_monitor_impl.cpp'
            )
        else
            libjami_sources += files(
                'media/video/v4l2/video_device_impl.cpp',
                'media/video/v4l2/video_device_monitor_impl.cpp'
            )
            libjami_dependencies += deplibudev
        endif
    elif host_machine.system() == 'darwin'
        if meson.get_compiler('cpp').compiles('''#import <TargetConditionals.h>
                                                 #if TARGET_OS_IPHONE == 0
                                                 #error This is not an IPHONE
                                                 #endif''')
            libjami_sources += files(
                'media/video/iosvideo/video_device_impl.cpp',
                'media/video/iosvideo/video_device_monitor_impl.cpp'
            )
        else
            libjami_sources += files(
                'media/video/osxvideo/video_device_impl.mm',
                'media/video/osxvideo/video_device_monitor_impl.mm'
            )
        endif
    elif host_machine.system() == 'windows'
        libjami_sources += files(
            'media/video/winvideo/video_device_impl.cpp',
            'media/video/winvideo/video_device_monitor_impl.cpp'
        )
    else
        error('Jami currently doesn\'t have support for video on @0@.\nPlease reconfigure the project with -Dvideo=false option.'.format(host_machine.system()))
    endif
endif

if conf.get('ENABLE_PLUGIN')
    libjami_sources += files(
        'client/plugin_manager_interface.cpp',
        'plugin/callservicesmanager.cpp',
        'plugin/chatservicesmanager.cpp',
        'plugin/jamipluginmanager.cpp',
        'plugin/pluginloader.cpp',
        'plugin/pluginmanager.cpp',
        'plugin/pluginpreferencesutils.cpp',
        'plugin/pluginsutils.cpp'
    )
    libjami_dependencies += [deplibarchive, depdl]
endif

libjami = library('jami',
    version: meson.project_version(),
    include_directories: libjami_includedirs,
    sources: libjami_sources,
    dependencies: libjami_dependencies,
    install: get_option('interfaces').contains('library')
)

libjami_dep = declare_dependency(
    include_directories: 'dring',
    link_with: libjami,
    variables: {
        'video': conf.get('ENABLE_VIDEO')? 'true' : 'false',
        'plugins': conf.get('ENABLE_PLUGIN')? 'true' : 'false'
    }
)

if get_option('interfaces').contains('library')
    install_headers(
        'dring/account_const.h',
        'dring/call_const.h',
        'dring/callmanager_interface.h',
        'dring/configurationmanager_interface.h',
        'dring/datatransfer_interface.h',
        'dring/conversation_interface.h',
        'dring/def.h',
        'dring/dring.h',
        'dring/media_const.h',
        'dring/presence_const.h',
        'dring/presencemanager_interface.h',
        'dring/security_const.h',
        subdir: 'dring'
    )
    if conf.get('ENABLE_VIDEO')
        install_headers(
            'dring/videomanager_interface.h',
            subdir: 'dring'
        )
    endif
    if conf.get('ENABLE_PLUGIN')
        install_headers(
            'dring/plugin_manager_interface.h',
            subdir: 'dring'
        )
    endif

    modpkgconfig.generate(
        libjami,
        name: 'jami',
        version: meson.project_version(),
        description: 'The Jami core library',
        url: 'https://jami.net/',
        subdirs: 'dring',
        filebase: 'jami',
        variables: [
            'video=' + (conf.get('ENABLE_VIDEO')? 'true' : 'false'),
            'plugins=' + (conf.get('ENABLE_PLUGIN')? 'true' : 'false')
        ]
    )
endif
