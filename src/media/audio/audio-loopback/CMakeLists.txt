################################################################################
# Source groups - audio_loopback
################################################################################
list (APPEND Source_Files__media__audio__audio_loopback
      "${CMAKE_CURRENT_SOURCE_DIR}/wasapi_loopback_capture.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/wasapi_loopback_capture.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/Common.h"
)

set (Source_Files__media__audio__audio_loopback ${Source_Files__media__audio__audio_loopback} PARENT_SCOPE)