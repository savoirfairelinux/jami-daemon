################################################################################
# Source groups - msvc
################################################################################

if(MSVC)
   list (APPEND Source_Files
         "${CMAKE_CURRENT_SOURCE_DIR}/config.h"
         "${CMAKE_CURRENT_SOURCE_DIR}/dlfcn.c"
         "${CMAKE_CURRENT_SOURCE_DIR}/dlfcn.h"
   )
endif()

set (Source_Files ${Source_Files} PARENT_SCOPE)