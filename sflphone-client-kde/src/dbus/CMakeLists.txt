
MESSAGE("Yeeeeaaaaah : rentre dans dbus!!!!!")
# Build dbus interfaces

# configuration manager interface

SET ( configurationmanager_xml  ${CMAKE_CURRENT_SOURCE_DIR}/../sflphone-common/src/dbus/configurationmanager-introspec.xml )

SET_SOURCE_FILES_PROPERTIES(
   ${configurationmanager_xml}
   PROPERTIES 
   CLASSNAME ConfigurationManagerInterface
   INCLUDE "metatypes.h") 

QT4_ADD_DBUS_INTERFACE(
   sflphone_client_kde_SRCS 
   ${configurationmanager_xml}
   configurationmanager_dbus_interface)

# call manager interface

SET ( callmanager_xml  ${CMAKE_CURRENT_SOURCE_DIR}/../sflphone-common/src/dbus/callmanager-introspec.xml )

SET_SOURCE_FILES_PROPERTIES(
   ${callmanager_xml}
   PROPERTIES 
   CLASSNAME CallManagerInterface
   INCLUDE "metatypes.h") 

QT4_ADD_DBUS_INTERFACE(
   sflphone_client_kde_SRCS 
   ${callmanager_xml}
   callmanager_dbus_interface)

# instance interface

SET ( instance_xml  ${CMAKE_CURRENT_SOURCE_DIR}/../sflphone-common/src/dbus/instance-introspec.xml )

SET_SOURCE_FILES_PROPERTIES(
   ${instance_xml}
   PROPERTIES 
   CLASSNAME InstanceInterface
   INCLUDE "metatypes.h") 
   
QT4_ADD_DBUS_INTERFACE(
   sflphone_client_kde_SRCS 
   ${instance_xml}
   instance_dbus_interface)
