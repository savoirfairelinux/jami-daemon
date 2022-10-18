################################################################################
# Source groups - upnp
################################################################################

# protocol

list (APPEND Source_Files__upnp
      "${CMAKE_CURRENT_SOURCE_DIR}/upnp_context.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/upnp_context.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/upnp_control.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/upnp_control.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/upnp_thread_util.h"
)

set (Source_Files__upnp ${Source_Files__upnp} PARENT_SCOPE)

add_subdirectory(protocol)

set (Source_Files__upnp__protocol ${Source_Files__upnp__protocol} PARENT_SCOPE)
set (Source_Files__upnp__protocol__pupnp ${Source_Files__upnp__protocol__pupnp} PARENT_SCOPE)
set (Source_Files__upnp__protocol__natpmp ${Source_Files__upnp__protocol__natpmp} PARENT_SCOPE)
