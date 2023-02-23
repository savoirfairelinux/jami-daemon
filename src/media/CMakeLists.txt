################################################################################
# Source groups - media
################################################################################

# audio|video

list (APPEND Source_Files__media
      "${CMAKE_CURRENT_SOURCE_DIR}/congestion_control.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/congestion_control.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/decoder_finder.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/libav_deps.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/libav_utils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/libav_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/localrecorder.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/localrecorder.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/localrecordermanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/localrecordermanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_attribute.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_attribute.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_buffer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_buffer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_codec.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_codec.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_decoder.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_decoder.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_device.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_encoder.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_encoder.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_filter.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_filter.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_io_handle.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_io_handle.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_player.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_player.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_recorder.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_recorder.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_stream.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/recordable.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/recordable.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/rtp_session.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/socket_pair.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/socket_pair.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/srtp.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/srtp.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/system_codec_container.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/system_codec_container.h"
)

set (Source_Files__media ${Source_Files__media} PARENT_SCOPE)

add_subdirectory(audio)
add_subdirectory(video)

set (Source_Files__media__audio ${Source_Files__media__audio} PARENT_SCOPE)
if(MSVC)
   set (Source_Files__media__audio__portaudio ${Source_Files__media__audio__portaudio} PARENT_SCOPE)
endif()
set (Source_Files__media__audio__sound ${Source_Files__media__audio__sound} PARENT_SCOPE)
set (Source_Files__media__audio__audio_processing ${Source_Files__media__audio__audio_processing} PARENT_SCOPE)
set (Source_Files__media__audio__webrtc ${Source_Files__media__audio__webrtc} PARENT_SCOPE)

set (Source_Files__media__video ${Source_Files__media__video} PARENT_SCOPE)
if(MSVC)
   if(WINDOWS_STORE)
      set (Source_Files__media__video__uwpvideo ${Source_Files__media__video__uwpvideo} PARENT_SCOPE)
   endif()

   set (Source_Files__media__video__winvideo ${Source_Files__media__video__winvideo} PARENT_SCOPE)
endif()
