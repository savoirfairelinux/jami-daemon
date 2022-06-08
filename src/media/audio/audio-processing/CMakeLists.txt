################################################################################
# Source groups - echo-cancel
################################################################################
list (APPEND Source_Files__media__audio__echo_cancel
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_processor.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/null_audio_processor.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/null_audio_processor.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/speex.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/speex.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/webrtc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/webrtc.cpp"
)

set (Source_Files__media__audio__echo_cancel ${Source_Files__media__audio__echo_cancel} PARENT_SCOPE)
