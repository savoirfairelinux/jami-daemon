################################################################################
# Source groups - audio-processing
################################################################################
list (APPEND Source_Files__media__audio__audio_processing
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_processor.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/null_audio_processor.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/null_audio_processor.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/speex.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/speex.cpp"
)

list (APPEND Source_Files__media__audio__webrtc
      "${CMAKE_CURRENT_SOURCE_DIR}/webrtc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/webrtc.cpp"
)

set (Source_Files__media__audio__audio_processing ${Source_Files__media__audio__audio_processing} PARENT_SCOPE)
set (Source_Files__media__audio__webrtc ${Source_Files__media__audio__webrtc} PARENT_SCOPE)
