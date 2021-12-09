################################################################################
# Source groups - plugin
################################################################################
list (APPEND Source_Files__plugin
      "${CMAKE_CURRENT_SOURCE_DIR}/jamipluginmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginloader.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginpreferencesutils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginsutils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/preferenceservicesmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/preferenceservicesmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/callservicesmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/callservicesmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/chatservicesmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/chatservicesmanager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/preferencehandler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/chathandler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/jamiplugin.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/jamipluginmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginsutils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/mediahandler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginloader.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginmanager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/pluginpreferencesutils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/streamdata.h"
)

set (Source_Files__plugin ${Source_Files__plugin} PARENT_SCOPE)