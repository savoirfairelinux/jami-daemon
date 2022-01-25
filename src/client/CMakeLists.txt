################################################################################
# Source groups - client
################################################################################
list (APPEND Source_Files__client
      "${CMAKE_CURRENT_SOURCE_DIR}/callmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/configurationmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/datatransfer.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/presencemanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ring_signal.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/ring_signal.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/videomanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/videomanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/plugin_manager_interface.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversation_interface.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/video_frame_buffer.cpp"
)

set (Source_Files__client ${Source_Files__client} PARENT_SCOPE)
