################################################################################
# Source groups - pipewire
################################################################################
list (APPEND Source_Files__media__audio__pipewire
      "${CMAKE_CURRENT_SOURCE_DIR}/loopbackcapture.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/loopbackcapture.h"
)

set (Source_Files__media__audio__portaudio ${Source_Files__media__audio__pipewire} PARENT_SCOPE)