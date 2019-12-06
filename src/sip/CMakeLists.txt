################################################################################
# Source groups - sip
################################################################################
list (APPEND Source_Files__sip
      "${CMAKE_CURRENT_SOURCE_DIR}/pres_sub_client.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pres_sub_client.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/pres_sub_server.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/pres_sub_server.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdes_negotiator.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdes_negotiator.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdp.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sdp.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sip_utils.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sip_utils.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sipaccount.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sipaccount.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sipaccountbase.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sipaccountbase.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sipcall.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sipcall.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sippresence.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sippresence.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/siptransport.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/siptransport.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sipvoiplink.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sipvoiplink.h"
)

set (Source_Files__sip ${Source_Files__sip} PARENT_SCOPE)