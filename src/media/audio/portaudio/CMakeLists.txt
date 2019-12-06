################################################################################
# Source groups - portaudio
################################################################################
list (APPEND Source_Files__media__audio__portaudio
      "${CMAKE_CURRENT_SOURCE_DIR}/portaudiolayer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/portaudiolayer.h"
)

set (Source_Files__media__audio__portaudio ${Source_Files__media__audio__portaudio} PARENT_SCOPE)