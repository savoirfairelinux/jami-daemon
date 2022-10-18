################################################################################
# Source groups - dhtnet
################################################################################
list (APPEND Source_Files__dhtnet
    "${CMAKE_CURRENT_SOURCE_DIR}/connectionmanager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/connectionmanager.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ice_socket.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/generic_io.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ice_transport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ice_transport.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ip_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ip_utils.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/multiplexed_socket.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/multiplexed_socket.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/peer_connection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/peer_connection.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/sip_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sip_utils.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/string_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/string_utils.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/utf8_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/utf8_utils.h"
)

set (Source_Files__dhtnet ${Source_Files__dhtnet} PARENT_SCOPE)

add_subdirectory(security)

set (Source_Files__security ${Source_Files__security} PARENT_SCOPE)

add_subdirectory(upnp)

set (Source_Files__upnp ${Source_Files__upnp} PARENT_SCOPE)
set (Source_Files__upnp__protocol ${Source_Files__upnp__protocol} PARENT_SCOPE)
set (Source_Files__upnp__protocol__pupnp ${Source_Files__upnp__protocol__pupnp} PARENT_SCOPE)
set (Source_Files__upnp__protocol__natpmp ${Source_Files__upnp__protocol__natpmp} PARENT_SCOPE)
