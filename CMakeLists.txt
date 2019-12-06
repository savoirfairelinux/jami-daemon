cmake_minimum_required(VERSION 3.15)

project(ring-daemon C CXX)

################################################################################
# Source groups
################################################################################
list (APPEND Source_Files
      "src/account.cpp"
      "src/account.h"
      "src/account_factory.cpp"
      "src/account_factory.h"
      "src/account_schema.h"
      "src/archiver.cpp"
      "src/archiver.h"
      "src/array_size.h"
      "src/base64.cpp"
      "src/base64.h"
      "src/buildinfo.cpp"
      "src/call.cpp"
      "src/call.h"
      "src/call_factory.cpp"
      "src/call_factory.h"
      "src/channel.h"
      "src/compiler_intrinsics.h"
      "src/conference.cpp"
      "src/conference.h"
      "src/data_transfer.cpp"
      "src/data_transfer.h"
      "src/enumclass_utils.h"
      "src/fileutils.cpp"
      "src/fileutils.h"
      "src/ftp_server.cpp"
      "src/ftp_server.h"
      "src/generic_io.h"
      "src/ice_socket.h"
      "src/ice_transport.cpp"
      "src/ice_transport.h"
      "src/ip_utils.cpp"
      "src/ip_utils.h"
      "src/logger.cpp"
      "src/logger.h"
      "src/manager.cpp"
      "src/manager.h"
      "src/map_utils.h"
      "src/noncopyable.h"
      "src/peer_connection.cpp"
      "src/peer_connection.h"
      "src/preferences.cpp"
      "src/preferences.h"
      "src/rational.h"
      "src/registration_states.h"
      "src/ring_api.cpp"
      "src/ring_types.h"
      "src/rw_mutex.h"
      "src/scheduled_executor.cpp"
      "src/scheduled_executor.h"
      "src/smartools.cpp"
      "src/smartools.h"
      "src/string_utils.cpp"
      "src/string_utils.h"
      "src/threadloop.cpp"
      "src/threadloop.h"
      "src/turn_transport.cpp"
      "src/turn_transport.h"
      "src/utf8_utils.cpp"
      "src/utf8_utils.h"
)

if(MSVC)
   list (APPEND Source_Files
         "src/windirent.h"
         "src/winsyslog.c"
         "src/winsyslog.h"
         "MSVC/config.h"
         "MSVC/dlfcn.c"
         "MSVC/dlfcn.h"
   )
endif()

source_group("Source Files" FILES ${Source_Files})

list (APPEND Source_Files__client
      "src/client/callmanager.cpp"
      "src/client/configurationmanager.cpp"
      "src/client/datatransfer.cpp"
      "src/client/presencemanager.cpp"
      "src/client/ring_signal.cpp"
      "src/client/ring_signal.h"
      "src/client/videomanager.cpp"
      "src/client/videomanager.h"
)
source_group("Source Files\\client" FILES ${Source_Files__client})

list (APPEND Source_Files__config
      "src/config/serializable.h"
      "src/config/yamlparser.cpp"
      "src/config/yamlparser.h"
)
source_group("Source Files\\config" FILES ${Source_Files__config})

list (APPEND Source_Files__dring
      "src/dring/account_const.h"
      "src/dring/call_const.h"
      "src/dring/callmanager_interface.h"
      "src/dring/configurationmanager_interface.h"
      "src/dring/datatransfer_interface.h"
      "src/dring/dring.h"
      "src/dring/media_const.h"
      "src/dring/presence_const.h"
      "src/dring/presencemanager_interface.h"
      "src/dring/security_const.h"
      "src/dring/videomanager_interface.h"
)
source_group("Source Files\\dring" FILES ${Source_Files__dring})

list (APPEND Source_Files__hooks
      "src/hooks/urlhook.cpp"
      "src/hooks/urlhook.h"
)
source_group("Source Files\\hooks" FILES ${Source_Files__hooks})

list (APPEND Source_Files__im
      "src/im/instant_messaging.cpp"
      "src/im/instant_messaging.h"
      "src/im/message_engine.cpp"
      "src/im/message_engine.h"
)
source_group("Source Files\\im" FILES ${Source_Files__im})

list (APPEND Source_Files__jamidht
      "src/jamidht/account_manager.cpp"
      "src/jamidht/account_manager.h"
      "src/jamidht/accountarchive.cpp"
      "src/jamidht/accountarchive.h"
      "src/jamidht/archive_account_manager.cpp"
      "src/jamidht/archive_account_manager.h"
      "src/jamidht/configkeys.h"
      "src/jamidht/connectionmanager.cpp"
      "src/jamidht/connectionmanager.h"
      "src/jamidht/contact_list.cpp"
      "src/jamidht/contact_list.h"
      "src/jamidht/jami_contact.h"
      "src/jamidht/jamiaccount.cpp"
      "src/jamidht/jamiaccount.h"
      "src/jamidht/multiplexed_socket.cpp"
      "src/jamidht/multiplexed_socket.h"
      "src/jamidht/namedirectory.cpp"
      "src/jamidht/namedirectory.h"
      "src/jamidht/p2p.cpp"
      "src/jamidht/p2p.h"
      "src/jamidht/jami_contact.h"
      "src/jamidht/server_account_manager.cpp"
      "src/jamidht/server_account_manager.h"
      "src/jamidht/sips_transport_ice.cpp"
      "src/jamidht/sips_transport_ice.h"
)
source_group("Source Files\\jamidht" FILES ${Source_Files__jamidht})

list (APPEND Source_Files__jamidht__eth__libdevcore
      "src/jamidht/eth/libdevcore/Address.h"
      "src/jamidht/eth/libdevcore/Common.cpp"
      "src/jamidht/eth/libdevcore/Common.h"
      "src/jamidht/eth/libdevcore/CommonData.cpp"
      "src/jamidht/eth/libdevcore/CommonData.h"
      "src/jamidht/eth/libdevcore/FixedHash.cpp"
      "src/jamidht/eth/libdevcore/FixedHash.h"
      "src/jamidht/eth/libdevcore/Makefile.am"
      "src/jamidht/eth/libdevcore/SHA3.cpp"
      "src/jamidht/eth/libdevcore/SHA3.h"
      "src/jamidht/eth/libdevcore/vector_ref.h"
)
source_group("Source Files\\jamidht\\eth\\libdevcore" FILES ${Source_Files__jamidht__eth__libdevcore})

list (APPEND Source_Files__jamidht__eth__libdevcrypto
      "src/jamidht/eth/libdevcrypto/Common.cpp"
      "src/jamidht/eth/libdevcrypto/Common.h"
)
source_group("Source Files\\jamidht\\eth\\libdevcrypto" FILES ${Source_Files__jamidht__eth__libdevcrypto})

list (APPEND Source_Files__media
      "src/media/congestion_control.cpp"
      "src/media/congestion_control.h"
      "src/media/decoder_finder.h"
      "src/media/libav_deps.h"
      "src/media/libav_utils.cpp"
      "src/media/libav_utils.h"
      "src/media/localrecorder.cpp"
      "src/media/localrecorder.h"
      "src/media/localrecordermanager.cpp"
      "src/media/localrecordermanager.h"
      "src/media/media_buffer.cpp"
      "src/media/media_buffer.h"
      "src/media/media_codec.cpp"
      "src/media/media_codec.h"
      "src/media/media_decoder.cpp"
      "src/media/media_decoder.h"
      "src/media/media_device.h"
      "src/media/media_encoder.cpp"
      "src/media/media_encoder.h"
      "src/media/media_filter.cpp"
      "src/media/media_filter.h"
      "src/media/media_io_handle.cpp"
      "src/media/media_io_handle.h"
      "src/media/media_recorder.cpp"
      "src/media/media_recorder.h"
      "src/media/media_stream.h"
      "src/media/recordable.cpp"
      "src/media/recordable.h"
      "src/media/rtp_session.h"
      "src/media/socket_pair.cpp"
      "src/media/socket_pair.h"
      "src/media/srtp.c"
      "src/media/srtp.h"
      "src/media/system_codec_container.cpp"
      "src/media/system_codec_container.h"
)
source_group("Source Files\\media" FILES ${Source_Files__media})

list (APPEND Source_Files__media__audio
      "src/media/audio/audio_frame_resizer.cpp"
      "src/media/audio/audio_frame_resizer.h"
      "src/media/audio/audio_input.cpp"
      "src/media/audio/audio_input.h"
      "src/media/audio/audio_receive_thread.cpp"
      "src/media/audio/audio_receive_thread.h"
      "src/media/audio/audio_rtp_session.cpp"
      "src/media/audio/audio_rtp_session.h"
      "src/media/audio/audio_sender.cpp"
      "src/media/audio/audio_sender.h"
      "src/media/audio/audiobuffer.cpp"
      "src/media/audio/audiobuffer.h"
      "src/media/audio/audiolayer.cpp"
      "src/media/audio/audiolayer.h"
      "src/media/audio/audioloop.cpp"
      "src/media/audio/audioloop.h"
      "src/media/audio/dcblocker.cpp"
      "src/media/audio/dcblocker.h"
      "src/media/audio/resampler.cpp"
      "src/media/audio/resampler.h"
      "src/media/audio/ringbuffer.cpp"
      "src/media/audio/ringbuffer.h"
      "src/media/audio/ringbufferpool.cpp"
      "src/media/audio/ringbufferpool.h"
      "src/media/audio/tonecontrol.cpp"
      "src/media/audio/tonecontrol.h"
)

if(NOT MSVC)
   list (APPEND Source_Files__media__audio
         "src/media/audio/dsp.cpp"
         "src/media/audio/dsp.h"
   )
endif()

source_group("Source Files\\media\\audio" FILES ${Source_Files__media__audio})

if(MSVC)
   list (APPEND Source_Files__media__audio__portaudio
         "src/media/audio/portaudio/portaudiolayer.cpp"
         "src/media/audio/portaudio/portaudiolayer.h"
   )
   source_group("Source Files\\media\\audio\\portaudio" FILES ${Source_Files__media__audio__portaudio})
endif()

list (APPEND Source_Files__media__audio__sound
      "src/media/audio/sound/audiofile.cpp"
      "src/media/audio/sound/audiofile.h"
      "src/media/audio/sound/dtmf.cpp"
      "src/media/audio/sound/dtmf.h"
      "src/media/audio/sound/dtmfgenerator.cpp"
      "src/media/audio/sound/dtmfgenerator.h"
      "src/media/audio/sound/tone.cpp"
      "src/media/audio/sound/tone.h"
      "src/media/audio/sound/tonelist.cpp"
      "src/media/audio/sound/tonelist.h"
)
source_group("Source Files\\media\\audio\\sound" FILES ${Source_Files__media__audio__sound})

list (APPEND Source_Files__media__video
      "src/media/video/accel.cpp"
      "src/media/video/accel.h"
      "src/media/video/filter_transpose.cpp"
      "src/media/video/filter_transpose.h"
      "src/media/video/shm_header.h"
      "src/media/video/sinkclient.cpp"
      "src/media/video/sinkclient.h"
      "src/media/video/video_base.cpp"
      "src/media/video/video_base.h"
      "src/media/video/video_device.h"
      "src/media/video/video_device_monitor.cpp"
      "src/media/video/video_device_monitor.h"
      "src/media/video/video_input.cpp"
      "src/media/video/video_input.h"
      "src/media/video/video_mixer.cpp"
      "src/media/video/video_mixer.h"
      "src/media/video/video_receive_thread.cpp"
      "src/media/video/video_receive_thread.h"
      "src/media/video/video_rtp_session.cpp"
      "src/media/video/video_rtp_session.h"
      "src/media/video/video_scaler.cpp"
      "src/media/video/video_scaler.h"
      "src/media/video/video_sender.cpp"
      "src/media/video/video_sender.h"
)
source_group("Source Files\\media\\video" FILES ${Source_Files__media__video})

if(MSVC)
   if(WINDOWS_STORE)
      list (APPEND Source_Files__media__video__uwpvideo
            "src/media/video/uwpvideo/video_device_impl.cpp"
            "src/media/video/uwpvideo/video_device_monitor_impl.cpp"
      )
      source_group("Source Files\\media\\video\\uwpvideo" FILES ${Source_Files__media__video__uwpvideo})
   endif()

   list (APPEND Source_Files__media__video__winvideo
         "src/media/video/winvideo/capture_graph_interfaces.h"
         "src/media/video/winvideo/video_device_impl.cpp"
         "src/media/video/winvideo/video_device_monitor_impl.cpp"
   )
   source_group("Source Files\\media\\video\\winvideo" FILES ${Source_Files__media__video__winvideo})
endif()

list (APPEND Source_Files__security
      "src/security/certstore.cpp"
      "src/security/certstore.h"
      "src/security/diffie-hellman.cpp"
      "src/security/diffie-hellman.h"
      "src/security/memory.cpp"
      "src/security/memory.h"
      "src/security/tls_session.cpp"
      "src/security/tls_session.h"
      "src/security/tlsvalidator.cpp"
      "src/security/tlsvalidator.h"
)
source_group("Source Files\\security" FILES ${Source_Files__security})

list (APPEND Source_Files__sip
      "src/sip/pres_sub_client.cpp"
      "src/sip/pres_sub_client.h"
      "src/sip/pres_sub_server.cpp"
      "src/sip/pres_sub_server.h"
      "src/sip/sdes_negotiator.cpp"
      "src/sip/sdes_negotiator.h"
      "src/sip/sdp.cpp"
      "src/sip/sdp.h"
      "src/sip/sip_utils.cpp"
      "src/sip/sip_utils.h"
      "src/sip/sipaccount.cpp"
      "src/sip/sipaccount.h"
      "src/sip/sipaccountbase.cpp"
      "src/sip/sipaccountbase.h"
      "src/sip/sipcall.cpp"
      "src/sip/sipcall.h"
      "src/sip/sippresence.cpp"
      "src/sip/sippresence.h"
      "src/sip/siptransport.cpp"
      "src/sip/siptransport.h"
      "src/sip/sipvoiplink.cpp"
      "src/sip/sipvoiplink.h"
)
source_group("Source Files\\sip" FILES ${Source_Files__sip})

list (APPEND Source_Files__upnp
      "src/upnp/upnp_context.cpp"
      "src/upnp/upnp_context.h"
      "src/upnp/upnp_control.cpp"
      "src/upnp/upnp_control.h"
)
source_group("Source Files\\upnp" FILES ${Source_Files__upnp})

list (APPEND Source_Files__upnp__protocol
      "src/upnp/protocol/global_mapping.h"
      "src/upnp/protocol/igd.cpp"
      "src/upnp/protocol/igd.h"
      "src/upnp/protocol/mapping.cpp"
      "src/upnp/protocol/mapping.h"
      "src/upnp/protocol/upnp_protocol.h"
)
source_group("Source Files\\upnp\\protocol" FILES ${Source_Files__upnp__protocol})

list (APPEND Source_Files__upnp__protocol__natpmp
      "src/upnp/protocol/natpmp/nat_pmp.cpp"
      "src/upnp/protocol/natpmp/nat_pmp.h"
      "src/upnp/protocol/natpmp/pmp_igd.cpp"
      "src/upnp/protocol/natpmp/pmp_igd.h"
)
source_group("Source Files\\upnp\\protocol\\natpmp" FILES ${Source_Files__upnp__protocol__natpmp})

list (APPEND Source_Files__upnp__protocol__pupnp
      "src/upnp/protocol/pupnp/pupnp.cpp"
      "src/upnp/protocol/pupnp/pupnp.h"
      "src/upnp/protocol/pupnp/upnp_igd.cpp"
      "src/upnp/protocol/pupnp/upnp_igd.h"
)
source_group("Source Files\\upnp\\protocol\\pupnp" FILES ${Source_Files__upnp__protocol__pupnp})

list (APPEND ALL_FILES
      ${Source_Files}
      ${Source_Files__client}
      ${Source_Files__config}
      ${Source_Files__dring}
      ${Source_Files__hooks}
      ${Source_Files__im}
      ${Source_Files__jamidht}
      ${Source_Files__jamidht__eth__libdevcore}
      ${Source_Files__jamidht__eth__libdevcrypto}
      ${Source_Files__media}
      ${Source_Files__media__audio}
      ${Source_Files__media__audio__sound}
      ${Source_Files__media__video}
      ${Source_Files__security}
      ${Source_Files__sip}
      ${Source_Files__upnp}
      ${Source_Files__upnp__protocol}
      ${Source_Files__upnp__protocol__natpmp}
      ${Source_Files__upnp__protocol__pupnp}
)

if(MSVC)
   list (APPEND ALL_FILES ${ALL_FILES}
         "${Source_Files__media__audio__portaudio}"
         "${Source_Files__media__video__uwpvideo}"
         "${Source_Files__media__video__winvideo}"
   )

   ################################################################################
   # Target
   ################################################################################
   include("CMake/Utils.cmake")

   add_library(${PROJECT_NAME} STATIC ${ALL_FILES})
   set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "daemon")

   use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")

   ################################################################################
   # Include necessary .cmake file (need to init use_props first)
   ################################################################################
   include("CMake/Default.cmake")
   include("CMake/DefaultCXX.cmake")

   set(ROOT_NAMESPACE ringdaemon)

   ################################################################################
   # Target name
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       set_target_properties(${PROJECT_NAME} PROPERTIES
           TARGET_NAME_RELEASELIB_WIN32 "dring"
       )
   endif()
   ################################################################################
   # Output directory
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       set_target_properties(${PROJECT_NAME} PROPERTIES
           OUTPUT_DIRECTORY_RELEASELIB_WIN32 "${CMAKE_CURRENT_SOURCE_DIR}/MSVC/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/bin/"
       )
   endif()
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       set_target_properties(${PROJECT_NAME} PROPERTIES
           INTERPROCEDURAL_OPTIMIZATION_RELEASELIB_WIN32 "FALSE"
       )
   endif()
   ################################################################################
   # Include directories
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       target_include_directories(${PROJECT_NAME} PUBLIC
           "$<$<CONFIG:ReleaseLib_win32>:"
               "${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/include/upnp;"
               "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/include;"
               "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/sndfile/src;"
               "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/openssl/inc32;"
               "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/asio/asio/include;"
               "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/restinio/dev;"
               "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/fmt/include;"
               "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/http_parser;"
               "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/natpmp"
           ">"
           "${CMAKE_CURRENT_SOURCE_DIR}/.;"
           "${CMAKE_CURRENT_SOURCE_DIR}/MSVC;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/client;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/config;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/dring;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/hooks;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/im;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/media;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/jamidht;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/security;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/sip;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/upnp;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/upnp/igd;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/upnp/protocol;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/upnp/mapping;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/jamidht/eth;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/msgpack-c/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/jsoncpp/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/yaml-cpp/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjlib/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjnath/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjlib-util/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/third_party;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjmedia/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/libgit2/include"
       )
   endif()

   ################################################################################
   # Compile definitions
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       target_compile_definitions(${PROJECT_NAME} PRIVATE
           "$<$<CONFIG:ReleaseLib_win32>:"
               "_WIN32_WINNT=0x0A00;"
               "ASIO_STANDALONE;"
               "STATIC_GETOPT;"
               "OPENDHT_PROXY_CLIENT;"
               "OPENDHT_PROXY_SERVER;"
               "OPENDHT_PUSH_NOTIFICATIONS;"
               "NATPMP_STATICLIB;"
           ">"
           "_USE_MATH_DEFINES;"
           "_SCL_SECURE_NO_WARNINGS;"
           "_CRT_SECURE_NO_WARNINGS;"
           "NOMINMAX;"
           "HAVE_CONFIG_H;"
           "WIN32_LEAN_AND_MEAN;"
       )
   endif()

   ################################################################################
   # Compile and link options
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       target_compile_options(${PROJECT_NAME} PRIVATE
           $<$<CONFIG:ReleaseLib_win32>:
               /O2;
               /Oi;
               ${DEFAULT_CXX_RUNTIME_LIBRARY};
               /Gy;
               /MP;
               /Oy-;
               /sdl-;
               /W0;
               /FC
           >
           /nologo;
           /Zi;
           /wd4996;
           /wd4503;
           /wd4180;
           /wd4244;
           /wd4267;
           ${DEFAULT_CXX_EXCEPTION_HANDLING}
       )
   endif()
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       target_link_options(${PROJECT_NAME} PRIVATE
           $<$<CONFIG:ReleaseLib_win32>:
               /MACHINE:X86;
               /OPT:REF;
               /OPT:ICF
           >
           /VERBOSE:LIB;
           /NODEFAULTLIB:libcmtd;
           /NODEFAULTLIB:libcmt;
           /OPT:NOLBR;
           /OPT:NOICF;
           /LTCG
       )
   endif()

   ################################################################################
   # Dependencies
   ################################################################################

   set(libAdditionalDependencies "${CMAKE_STATIC_LINKER_FLAGS} /LTCG ws2_32.lib 
   advapi32.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avcodec.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avdevice.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avfilter.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avformat.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avutil.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/swresample.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/swscale.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libgnutls.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/lib_json.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libopendht.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/argon2/vs2015/Argon2Ref/vs2015/build/Argon2Ref.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/secp256k1.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/yaml-cpp/msvc/Release/libyaml-cppmd.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/portaudio.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libupnp.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/natpmp/msvc/Release/natpmp.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsip-core-x86_64-x64-vc15-Release.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsip-simple-x86_64-x64-vc15-Release.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsua2-lib-x86_64-x64-vc15-Release.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsua-lib-x86_64-x64-vc15-Release.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsip-ua-x86_64-x64-vc15-Release.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjmedia/lib/pjmedia-x86_64-x64-vc15-Release.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjlib-util/lib/pjlib-util-x86_64-x64-vc15-Release.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjlib/lib/pjlib-x86_64-x64-vc15-Release.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjnath/lib/pjnath-x86_64-x64-vc15-Release.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/fmt/msvc/Release/fmt.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/http_parser/x64/Release/http-parser.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/asio/asio/msvc/x64/Release/asio.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/openssl/out32dll/libeay32.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/openssl/out32dll/ssleay32.lib 
   ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/libgit2/msvc/Release/git2.lib 
   /ignore:4006 
   "
   )

   set ( CMAKE_STATIC_LINKER_FLAGS ${libAdditionalDependencies} )

endif()

