################################################################################
# Source groups - jami
################################################################################
list (APPEND Source_Files__jami
      "${CMAKE_CURRENT_SOURCE_DIR}/account_const.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/call_const.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/callmanager_interface.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/plugin_manager_interface.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/configurationmanager_interface.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversation_interface.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/datatransfer_interface.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/jami.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/media_const.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/presence_const.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/presencemanager_interface.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/videomanager_interface.h"
)

set (Source_Files__jami ${Source_Files__jami} PARENT_SCOPE)