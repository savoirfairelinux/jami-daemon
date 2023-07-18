################################################################################
# Source groups - connectivity
################################################################################
list (APPEND Source_Files__connectivity
    "${CMAKE_CURRENT_SOURCE_DIR}/sip_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sip_utils.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/utf8_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/utf8_utils.h"
)

set (Source_Files__connectivity ${Source_Files__connectivity} PARENT_SCOPE)

add_subdirectory(security)

set (Source_Files__security ${Source_Files__security} PARENT_SCOPE)
