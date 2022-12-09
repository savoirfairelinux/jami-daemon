################################################################################
# Source groups - src
################################################################################

# client|config|connectivity|jami.hooks|im|jamidht|media

list (APPEND Source_Files
      "${CMAKE_CURRENT_SOURCE_DIR}/account.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/account.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_config.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_config.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_factory.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_factory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_schema.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/archiver.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/archiver.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/base64.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/base64.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/buildinfo.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/call.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/call.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/call_factory.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/call_factory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/callstreamsmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/callstreamsmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/compiler_intrinsics.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/conference.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/conference.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/conference_protocol.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/conference_protocol.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/data_transfer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/data_transfer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/enumclass_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/fileutils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/fileutils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/gittransport.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/gittransport.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/logger.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/manager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/manager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/map_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/noncopyable.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/preferences.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/preferences.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/rational.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/registration_states.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ring_api.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ring_types.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/scheduled_executor.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/scheduled_executor.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/string_utils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/string_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/threadloop.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/threadloop.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/uri.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/uri.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/vcard.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/vcard.cpp"
)

if(MSVC)
   list (APPEND Source_Files
         "${CMAKE_CURRENT_SOURCE_DIR}/windirent.h"
         "${CMAKE_CURRENT_SOURCE_DIR}/winsyslog.c"
         "${CMAKE_CURRENT_SOURCE_DIR}/winsyslog.h"
   )
endif()

set (Source_Files ${Source_Files} PARENT_SCOPE)

add_subdirectory(client)
add_subdirectory(config)
add_subdirectory(connectivity)
add_subdirectory(jami)
add_subdirectory(im)
add_subdirectory(jamidht)
add_subdirectory(media)
add_subdirectory(plugin)
add_subdirectory(sip)

set (Source_Files__client ${Source_Files__client} PARENT_SCOPE)
set (Source_Files__config ${Source_Files__config} PARENT_SCOPE)
set (Source_Files__connectivity ${Source_Files__connectivity} PARENT_SCOPE)
set (Source_Files__jami ${Source_Files__jami} PARENT_SCOPE)
set (Source_Files__hooks ${Source_Files__hooks} PARENT_SCOPE)
set (Source_Files__im ${Source_Files__im} PARENT_SCOPE)
set (Source_Files__jamidht ${Source_Files__jamidht} PARENT_SCOPE)
set (Source_Files__jamidht__eth__libdevcore ${Source_Files__jamidht__eth__libdevcore} PARENT_SCOPE)
set (Source_Files__jamidht__eth__libdevcrypto ${Source_Files__jamidht__eth__libdevcrypto} PARENT_SCOPE)
set (Source_Files__jamidht__swarm ${Source_Files__jamidht__swarm} PARENT_SCOPE)
set (Source_Files__media ${Source_Files__media} PARENT_SCOPE)
set (Source_Files__media__audio ${Source_Files__media__audio} PARENT_SCOPE)
set (Source_Files__media__audio__sound ${Source_Files__media__audio__sound} PARENT_SCOPE)
set (Source_Files__media__audio__audio_processing ${Source_Files__media__audio__audio_processing} PARENT_SCOPE)
set (Source_Files__media__audio__webrtc ${Source_Files__media__audio__webrtc} PARENT_SCOPE)
set (Source_Files__media__video ${Source_Files__media__video} PARENT_SCOPE)
if(MSVC)
   set (Source_Files__media__audio__portaudio ${Source_Files__media__audio__portaudio} PARENT_SCOPE)
   set (Source_Files__media__video__winvideo ${Source_Files__media__video__winvideo} PARENT_SCOPE)
endif()
set (Source_Files__security ${Source_Files__security} PARENT_SCOPE)
set (Source_Files__sip ${Source_Files__sip} PARENT_SCOPE)
set (Source_Files__upnp ${Source_Files__upnp} PARENT_SCOPE)
set (Source_Files__upnp__protocol ${Source_Files__upnp__protocol} PARENT_SCOPE)
set (Source_Files__upnp__protocol__pupnp ${Source_Files__upnp__protocol__pupnp} PARENT_SCOPE)
set (Source_Files__upnp__protocol__natpmp ${Source_Files__upnp__protocol__natpmp} PARENT_SCOPE)
set (Source_Files__plugin ${Source_Files__plugin} PARENT_SCOPE)
