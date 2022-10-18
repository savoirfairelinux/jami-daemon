################################################################################
# Source groups - pupnp
################################################################################
list (APPEND Source_Files__upnp__protocol__pupnp
      "${CMAKE_CURRENT_SOURCE_DIR}/pupnp.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pupnp.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/upnp_igd.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/upnp_igd.h"
)

set (Source_Files__upnp__protocol__pupnp ${Source_Files__upnp__protocol__pupnp} PARENT_SCOPE)