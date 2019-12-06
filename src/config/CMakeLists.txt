################################################################################
# Source groups - config
################################################################################
list (APPEND Source_Files__config
      "${CMAKE_CURRENT_SOURCE_DIR}/serializable.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/yamlparser.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/yamlparser.h"
)

set (Source_Files__config ${Source_Files__config} PARENT_SCOPE)