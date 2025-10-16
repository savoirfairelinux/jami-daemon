################################################################################
# Source groups - pipewire
################################################################################
list (APPEND Source_Files__media__audio__pipewire_loopback_capture
      "${CMAKE_CURRENT_SOURCE_DIR}/pipewire_loopbackcapture.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pipewire_loopbackcapture.h"
)

set (Source_Files__media__audio__portaudio ${Source_Files__media__audio__pipewire_loopback_capture} PARENT_SCOPE)