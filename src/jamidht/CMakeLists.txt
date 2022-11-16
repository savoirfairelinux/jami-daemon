################################################################################
# Source groups - jamidht
################################################################################

# eth/libdevcore|eth/libdevcrypto

list (APPEND Source_Files__jamidht
      "${CMAKE_CURRENT_SOURCE_DIR}/abstract_sip_transport.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_manager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/account_manager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/accountarchive.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/accountarchive.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/archive_account_manager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/archive_account_manager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/configkeys.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversationrepository.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversationrepository.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversation.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversation.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/channeled_transport.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/channeled_transport.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/contact_list.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/contact_list.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/gitserver.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/gitserver.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/jami_contact.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/jamiaccount.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/jamiaccount.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/jamiaccount_config.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/jamiaccount_config.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/channel_handler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversation_channel_handler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversation_channel_handler.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/transfer_channel_handler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/transfer_channel_handler.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversation_module.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/conversation_module.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/namedirectory.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/namedirectory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/jami_contact.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/server_account_manager.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/server_account_manager.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sync_channel_handler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sync_channel_handler.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/sync_module.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/sync_module.cpp"
)

set (Source_Files__jamidht ${Source_Files__jamidht} PARENT_SCOPE)

add_subdirectory("swarm")
add_subdirectory("eth/libdevcore")
add_subdirectory("eth/libdevcrypto")

set (Source_Files__jamidht__swarm ${Source_Files__jamidht__swarm} PARENT_SCOPE)
set (Source_Files__jamidht__eth__libdevcore ${Source_Files__jamidht__eth__libdevcore} PARENT_SCOPE)
set (Source_Files__jamidht__eth__libdevcrypto ${Source_Files__jamidht__eth__libdevcrypto} PARENT_SCOPE)