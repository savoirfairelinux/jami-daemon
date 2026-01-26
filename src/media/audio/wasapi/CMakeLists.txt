################################################################################
# Source groups - wasapi
################################################################################
list (APPEND Source_Files__media__audio__wasapi
      "${CMAKE_CURRENT_SOURCE_DIR}/wasapiloopbackcapture.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/wasapiloopbackcapture.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/Common.h"
)

set (Source_Files__media__audio__wasapi ${Source_Files__media__audio__wasapi} PARENT_SCOPE)
