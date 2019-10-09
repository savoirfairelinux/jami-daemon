################################################################################
# Source groups - im
################################################################################
list (APPEND Source_Files__im
      "${CMAKE_CURRENT_SOURCE_DIR}/instant_messaging.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/instant_messaging.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/message_engine.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/message_engine.h"
)

set (Source_Files__im ${Source_Files__im} PARENT_SCOPE)