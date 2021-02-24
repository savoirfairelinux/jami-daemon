################################################################################
# Source groups - echo-cancel
################################################################################
list (APPEND Source_Files__media__audio__echo_cancel
      "${CMAKE_CURRENT_SOURCE_DIR}/echo_canceller.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/speex_echo_canceller.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/speex_echo_canceller.cpp"
)

set (Source_Files__media__audio__echo_cancel ${Source_Files__media__audio__echo_cancel} PARENT_SCOPE)