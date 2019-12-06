################################################################################
# Source groups - hooks
################################################################################
list (APPEND Source_Files__hooks
      "${CMAKE_CURRENT_SOURCE_DIR}/urlhook.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/urlhook.h"
)

set (Source_Files__hooks ${Source_Files__hooks} PARENT_SCOPE)