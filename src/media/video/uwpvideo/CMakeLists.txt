################################################################################
# Source groups - uwpvideo
################################################################################
list (APPEND Source_Files__media__video__uwpvideo
      "${CMAKE_CURRENT_SOURCE_DIR}/video_device_impl.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_device_monitor_impl.cpp"
)

set (Source_Files__media__video__uwpvideo ${Source_Files__media__video__uwpvideo} PARENT_SCOPE)
