################################################################################
# Source groups - protocol
################################################################################

# natpmp|pupnp

list (APPEND Source_Files__upnp__protocol
      "${CMAKE_CURRENT_SOURCE_DIR}/igd.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/igd.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/mapping.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/mapping.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/upnp_protocol.h"
)

set (Source_Files__upnp__protocol ${Source_Files__upnp__protocol} PARENT_SCOPE)

add_subdirectory(natpmp)
add_subdirectory(pupnp)

set (Source_Files__upnp__protocol__pupnp ${Source_Files__upnp__protocol__pupnp} PARENT_SCOPE)
set (Source_Files__upnp__protocol__natpmp ${Source_Files__upnp__protocol__natpmp} PARENT_SCOPE)
