################################################################################
# Source groups - libdevcore
################################################################################
list (APPEND Source_Files__jamidht__eth__libdevcore
      "${CMAKE_CURRENT_SOURCE_DIR}/Address.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/Common.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/CommonData.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/CommonData.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/FixedHash.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/Makefile.am"
      "${CMAKE_CURRENT_SOURCE_DIR}/SHA3.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/SHA3.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/vector_ref.h"
)

set (Source_Files__jamidht__eth__libdevcore ${Source_Files__jamidht__eth__libdevcore} PARENT_SCOPE)