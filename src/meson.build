libjami_includedirs = include_directories(
    '..',
    'jami',
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
    'client/videomanager.cpp',
    'config/yamlparser.cpp',
    'connectivity/security/memory.cpp',
    'connectivity/security/tlsvalidator.cpp',
    'connectivity/ip_utils.cpp',
    'connectivity/sip_utils.cpp',
    'connectivity/utf8_utils.cpp',
    'im/instant_messaging.cpp',
    'im/message_engine.cpp',
    'jamidht/eth/libdevcore/CommonData.cpp',
    'jamidht/eth/libdevcore/FixedHash.cpp',
    'jamidht/eth/libdevcore/SHA3.cpp',
    'jamidht/eth/libdevcrypto/Common.cpp',
    'jamidht/account_manager.cpp',
    'jamidht/accountarchive.cpp',
    'jamidht/archive_account_manager.cpp',
    'jamidht/channeled_transport.cpp',
    'jamidht/contact_list.cpp',
    'jamidht/conversation.cpp',
    'jamidht/conversation_channel_handler.cpp',
    'jamidht/conversation_module.cpp',
    'jamidht/conversationrepository.cpp',
    'jamidht/gitserver.cpp',
    'jamidht/jamiaccount.cpp',
    'jamidht/jamiaccount_config.cpp',
    'jamidht/namedirectory.cpp',
    'jamidht/server_account_manager.cpp',
    'jamidht/sync_channel_handler.cpp',
    'jamidht/sync_module.cpp',
    'jamidht/transfer_channel_handler.cpp',
    'jamidht/swarm/routing_table.cpp',
    'jamidht/swarm/swarm_manager.cpp',
    'jamidht/swarm/swarm_protocol.cpp',
    'jamidht/swarm/swarm_channel_handler.cpp',
    'media/audio/audio-processing/null_audio_processor.cpp',
    'media/audio/sound/audiofile.cpp',
    'media/audio/sound/dtmf.cpp',
    'media/audio/sound/dtmfgenerator.cpp',
    'media/audio/sound/tone.cpp',
    'media/audio/sound/tonelist.cpp',
    'media/audio/audio_frame_resizer.cpp',
    'media/audio/audio_input.cpp',
    'media/audio/audio_receive_thread.cpp',
    'media/audio/audio_rtp_session.cpp',
    'media/audio/audio_sender.cpp',
    'media/audio/audiolayer.cpp',
    'media/audio/audioloop.cpp',
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
    'sip/pres_sub_client.cpp',
    'sip/pres_sub_server.cpp',
    'sip/sdes_negotiator.cpp',
    'sip/sdp.cpp',
    'sip/sipaccount.cpp',
    'sip/sipaccount_config.cpp',
    'sip/sipaccountbase.cpp',
    'sip/sipaccountbase_config.cpp',
    'sip/sipcall.cpp',
    'sip/sippresence.cpp',
    'sip/siptransport.cpp',
    'sip/sipvoiplink.cpp',
    'account.cpp',
    'account_config.cpp',
    'account_factory.cpp',
    'archiver.cpp',
    'base64.cpp',
    'buildinfo.cpp',
    'call.cpp',
    'call_factory.cpp',
    'callstreamsmanager.cpp',
    'conference.cpp',
    'conference_protocol.cpp',
    'data_transfer.cpp',
    'fileutils.cpp',
    'gittransport.cpp',
    'logger.cpp',
    'manager.cpp',
    'preferences.cpp',
    'ring_api.cpp',
    'scheduled_executor.cpp',
    'string_utils.cpp',
    'threadloop.cpp',
    'uri.cpp',
    'vcard.cpp'
)
if host_machine.system() == 'windows'
    libjami_sources += files('winsyslog.c')
endif

libjami_dependencies = [
    depthreads,
    depopendht,
    depdhtnet,
    depgnutls,
    depnettle,
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

if conf.get('HAVE_RINGNS') == 1
    libjami_dependencies += depopenssl
endif

if conf.get('HAVE_WEBRTC_AP') == 1
    libjami_sources += files(
        'media/audio/audio-processing/webrtc.cpp'
    )
    libjami_dependencies += depwebrtcap
endif

if conf.get('HAVE_SPEEXDSP') == 1
    libjami_sources += files(
        'media/audio/audio-processing/speex.cpp'
    )
    libjami_dependencies += depspeexdsp
endif

if conf.get('ENABLE_VIDEO')
    libjami_sources += files(
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
        'plugin/pluginsutils.cpp',
        'plugin/preferenceservicesmanager.cpp',
        'plugin/webviewservicesmanager.cpp'
    )
    if host_machine.system() == 'darwin'
        libjami_dependencies += depminizip
    else
        libjami_dependencies += deplibarchive
    endif
    libjami_dependencies += depdl
endif

if conf.get('ENABLE_TRACEPOINTS')
    libjami_sources += files(
        'jami/tracepoint.c'
    )
    libjami_dependencies += deplttngust
endif

# https://ffmpeg.org/platform.html#Advanced-linking-configuration
libjami_linkargs = meson.get_compiler('cpp').get_supported_link_arguments(
    '-Wl,-Bsymbolic'
)

libjami = library('jami',
    version: meson.project_version(),
    include_directories: libjami_includedirs,
    sources: libjami_sources,
    dependencies: libjami_dependencies,
    link_args: libjami_linkargs,
    install: get_option('interfaces').contains('library')
)

depjami = declare_dependency(
    include_directories: 'jami',
    link_with: libjami,
    variables: {
        'video': conf.get('ENABLE_VIDEO')? 'true' : 'false',
        'plugins': conf.get('ENABLE_PLUGIN')? 'true' : 'false'
    }
)

if get_option('interfaces').contains('library')
    install_headers(
        'jami/account_const.h',
        'jami/call_const.h',
        'jami/callmanager_interface.h',
        'jami/configurationmanager_interface.h',
        'jami/conversation_interface.h',
        'jami/datatransfer_interface.h',
        'jami/def.h',
        'jami/jami.h',
        'jami/media_const.h',
        'jami/presence_const.h',
        'jami/presencemanager_interface.h',
        'jami/security_const.h',
        'jami/trace-tools.h',
        subdir: 'jami'
    )
    if conf.get('ENABLE_VIDEO')
        install_headers(
            'jami/videomanager_interface.h',
            subdir: 'jami'
        )
    endif
    if conf.get('ENABLE_PLUGIN')
        install_headers(
            'jami/plugin_manager_interface.h',
            subdir: 'jami'
        )
    endif

    modpkgconfig.generate(
        libjami,
        name: 'jami',
        version: meson.project_version(),
        description: 'The Jami core library',
        url: 'https://jami.net/',
        subdirs: 'jami',
        filebase: 'jami',
        variables: [
            'video=' + (conf.get('ENABLE_VIDEO')? 'true' : 'false'),
            'plugins=' + (conf.get('ENABLE_PLUGIN')? 'true' : 'false')
        ]
    )
endif
