################################################################################
# Source groups - sound
################################################################################
list (APPEND Source_Files__media__audio__sound
      "${CMAKE_CURRENT_SOURCE_DIR}/audiofile.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/audiofile.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/dtmf.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/dtmf.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/dtmfgenerator.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/dtmfgenerator.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tone.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/tone.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tonelist.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/tonelist.h"
)

set (Source_Files__media__audio__sound ${Source_Files__media__audio__sound} PARENT_SCOPE)