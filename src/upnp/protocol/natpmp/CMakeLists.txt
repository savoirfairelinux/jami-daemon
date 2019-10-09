################################################################################
# Source groups - natpmp
################################################################################
list (APPEND Source_Files__upnp__protocol__natpmp
      "${CMAKE_CURRENT_SOURCE_DIR}/nat_pmp.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/nat_pmp.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/pmp_igd.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pmp_igd.h"
)

set (Source_Files__upnp__protocol__natpmp ${Source_Files__upnp__protocol__natpmp} PARENT_SCOPE)