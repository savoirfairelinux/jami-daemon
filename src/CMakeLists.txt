################################################################################
# Source groups - src
################################################################################

# client|config|dring|hooks|im|jamidht|media|security|sip|transport|upnp

list (APPEND Source_Files
      "${CMAKE_CURRENT_SOURCE_DIR}/account.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/account.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_factory.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_factory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_schema.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/archiver.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/archiver.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/array_size.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/base64.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/base64.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/buildinfo.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/call.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/call.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/call_factory.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/call_factory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/compiler_intrinsics.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/conference.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/conference.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/data_transfer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/data_transfer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/enumclass_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/fileutils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/fileutils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/generic_io.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/gittransport.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/gittransport.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ice_socket.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ice_transport.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ice_transport.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ip_utils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ip_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/logger.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/manager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/manager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/map_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/noncopyable.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/peer_connection.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/peer_connection.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/preferences.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/preferences.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/rational.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/registration_states.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ring_api.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ring_types.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/rw_mutex.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/scheduled_executor.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/scheduled_executor.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/smartools.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/smartools.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/string_utils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/string_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/threadloop.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/threadloop.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/utf8_utils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/utf8_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/uri.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/uri.h"
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
add_subdirectory(dring)
add_subdirectory(im)
add_subdirectory(jamidht)
add_subdirectory(media)
add_subdirectory(security)
add_subdirectory(sip)
add_subdirectory(upnp)
add_subdirectory(plugin)

set (Source_Files__client ${Source_Files__client} PARENT_SCOPE)
set (Source_Files__config ${Source_Files__config} PARENT_SCOPE)
set (Source_Files__dring ${Source_Files__dring} PARENT_SCOPE)
set (Source_Files__hooks ${Source_Files__hooks} PARENT_SCOPE)
set (Source_Files__im ${Source_Files__im} PARENT_SCOPE)
set (Source_Files__jamidht ${Source_Files__jamidht} PARENT_SCOPE)
set (Source_Files__jamidht__eth__libdevcore ${Source_Files__jamidht__eth__libdevcore} PARENT_SCOPE)
set (Source_Files__jamidht__eth__libdevcrypto ${Source_Files__jamidht__eth__libdevcrypto} PARENT_SCOPE)
set (Source_Files__media ${Source_Files__media} PARENT_SCOPE)
set (Source_Files__media__audio ${Source_Files__media__audio} PARENT_SCOPE)
set (Source_Files__media__audio__sound ${Source_Files__media__audio__sound} PARENT_SCOPE)
set (Source_Files__media__audio__echo_cancel ${Source_Files__media__audio__echo_cancel} PARENT_SCOPE)
set (Source_Files__media__video ${Source_Files__media__video} PARENT_SCOPE)
if(MSVC)
   if(WINDOWS_STORE)
      set (Source_Files__media__video__uwpvideo ${Source_Files__media__video__uwpvideo} PARENT_SCOPE)
   endif()
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