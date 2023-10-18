################################################################################
# Source groups - video
################################################################################

# androidvideo|iosvideo|osxvideo|uwpvideo|v4l2|winvideo

list (APPEND Source_Files__media__video
      "${CMAKE_CURRENT_SOURCE_DIR}/accel.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/accel.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/filter_transpose.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/filter_transpose.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/shm_header.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sinkclient.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sinkclient.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_base.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_base.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_device.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_device_monitor.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_device_monitor.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_input.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_input.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_mixer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_mixer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_receive_thread.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_receive_thread.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_rtp_session.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_rtp_session.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_scaler.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_scaler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_sender.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_sender.h"
)

set (Source_Files__media__video ${Source_Files__media__video} PARENT_SCOPE)

if(MSVC)
   add_subdirectory(winvideo)
   set (Source_Files__media__video__winvideo ${Source_Files__media__video__winvideo} PARENT_SCOPE)
endif()

