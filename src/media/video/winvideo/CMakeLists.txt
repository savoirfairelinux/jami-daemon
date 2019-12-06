################################################################################
# Source groups - winvideo
################################################################################
list (APPEND Source_Files__media__video__winvideo
      "src/media/video/winvideo/capture_graph_interfaces.h"
      "src/media/video/winvideo/video_device_impl.cpp"
      "src/media/video/winvideo/video_device_monitor_impl.cpp"
)

set (Source_Files__media__video__winvideo ${Source_Files__media__video__winvideo} PARENT_SCOPE)
