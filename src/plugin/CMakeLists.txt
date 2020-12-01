################################################################################
# Source groups - plugin
################################################################################
list (APPEND Source_Files__plugin
      "${CMAKE_CURRENT_SOURCE_DIR}/jamipluginmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginloaderdl.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/callservicesmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/chatservicesmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/chathandler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/jamiplugin.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/jamipluginmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/mediahandler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginloader.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/streamdata.h"
)

set (Source_Files__plugin ${Source_Files__plugin} PARENT_SCOPE)