################################################################################
# Source groups - swarm
################################################################################
list (APPEND Source_Files__jamidht__swarm
      "${CMAKE_CURRENT_SOURCE_DIR}/routing_table.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/routing_table.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/swarm_channel_handler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/swarm_channel_handler.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/swarm_manager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/swarm_manager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/swarm_protocol.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/swarm_protocol.cpp"
)

set (Source_Files__jamidht__swarm ${Source_Files__jamidht__swarm} PARENT_SCOPE)