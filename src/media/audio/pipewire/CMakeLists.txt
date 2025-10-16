################################################################################
# Source groups - pipewire
################################################################################
list (APPEND Source_Files__media__audio__pipewire
      "${CMAKE_CURRENT_SOURCE_DIR}/pipeloopbackcapture.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pipeloopbackcapture.h"
)

set (Source_Files__media__audio__portaudio ${Source_Files__media__audio__pipewire} PARENT_SCOPE)