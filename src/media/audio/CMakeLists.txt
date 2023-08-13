################################################################################
# Source groups - audio
################################################################################

# alsa|coreaudio|jack|opensl|portaudio|pulseaudio|sound

list (APPEND Source_Files__media__audio
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_frame_resizer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_frame_resizer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_input.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_input.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_receive_thread.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_receive_thread.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_rtp_session.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_rtp_session.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_sender.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/audio_sender.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/audiolayer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/audiolayer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/audioloop.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/audioloop.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/resampler.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/resampler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ringbuffer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ringbuffer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/ringbufferpool.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ringbufferpool.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tonecontrol.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/tonecontrol.h"
)

set (Source_Files__media__audio ${Source_Files__media__audio} PARENT_SCOPE)

if(MSVC)
   add_subdirectory(portaudio)
   set (Source_Files__media__audio__portaudio ${Source_Files__media__audio__portaudio} PARENT_SCOPE)
endif()

add_subdirectory(sound)
set (Source_Files__media__audio__sound ${Source_Files__media__audio__sound} PARENT_SCOPE)

add_subdirectory(audio-processing)
set (Source_Files__media__audio__audio_processing ${Source_Files__media__audio__audio_processing} PARENT_SCOPE)
set (Source_Files__media__audio__webrtc ${Source_Files__media__audio__webrtc} PARENT_SCOPE)
