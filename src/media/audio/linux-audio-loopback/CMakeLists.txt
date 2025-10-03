################################################################################
# Source groups - linux-audio-loopback
################################################################################
list (APPEND Source_Files__media__audio__linux_audio_loopback
      "${CMAKE_CURRENT_SOURCE_DIR}/loopback_capture.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/loopback_capture.h"
)
set (Source_Files__media__audio__linux_audio_loopback ${Source_Files__media__audio__linux_audio_loopback} PARENT_SCOPE)