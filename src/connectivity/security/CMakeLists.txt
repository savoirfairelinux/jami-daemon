################################################################################
# Source groups - security
################################################################################
list (APPEND Source_Files__security
      "${CMAKE_CURRENT_SOURCE_DIR}/certstore.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/certstore.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/diffie-hellman.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/diffie-hellman.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/memory.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/memory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/security_const.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tls_session.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/tls_session.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tlsvalidator.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/tlsvalidator.h"
)

set (Source_Files__security ${Source_Files__security} PARENT_SCOPE)