

ADD_DEFINITIONS(${KDE4_DEFINITIONS} ${QT_DEFINITIONS} -fexceptions -DDATA_INSTALL_DIR="\\\"${DATA_INSTALL_DIR}\\\"" )

# add_definitions ( -DKDE_DEFAULT_DEBUG_AREA=9000 )

# kde4_set_debug_area( 1234 )

# IF(DEFINED DEBUG_DISABLED)
# MESSAGE("NO DEBUG OUTPUT")
# ADD_DEFINITIONS( -DQT_NO_DEBUG_OUTPUT )
# ENDIF(DEFINED DEBUG_DISABLED)


MESSAGE("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

IF(${CMAKE_BUILD_TYPE} MATCHES Release)
	MESSAGE("NO DEBUG OUTPUT")
	ADD_DEFINITIONS( -DQT_NO_DEBUG_OUTPUT )
ENDIF(${CMAKE_BUILD_TYPE} MATCHES Release)

SET ( KDE4_KABC_LIBS  -lkabc )

SET(
	sflphone_client_kde_SRCS
	sflphone_kdeview.cpp
	SFLPhone.cpp
	ConfigDialog.cpp
	main.cpp
	sflphone_const.h
	Account.cpp
	AccountList.cpp
	Call.cpp
	CallList.cpp
	configurationmanager_interface_singleton.cpp
	callmanager_interface_singleton.cpp
	instance_interface_singleton.cpp
	AccountWizard.cpp
	AccountItemWidget.cpp
	ActionSetAccountFirst.cpp
	Contact.cpp
	ContactItemWidget.cpp
	conf/ConfigurationDialog.cpp
	conf/dlggeneral.cpp
	conf/ConfigurationSkeleton.cpp
)
 

# generate rules for building source files from the resources
SET(QtApp_RCCS qrc/resources.qrc)
QT4_ADD_RESOURCES(QtApp_RCC_SRCS ${QtApp_RCCS})


# Build dbus interfaces
SET ( dbus_xml_introspecs_path ${CMAKE_CURRENT_SOURCE_DIR}/dbus/)

# configuration manager interface
SET ( configurationmanager_xml  ${dbus_xml_introspecs_path}/configurationmanager-introspec.xml )

SET_SOURCE_FILES_PROPERTIES(
   ${configurationmanager_xml}
   PROPERTIES 
   CLASSNAME ConfigurationManagerInterface
   INCLUDE "dbus/metatypes.h") 

QT4_ADD_DBUS_INTERFACE(
   sflphone_client_kde_SRCS 
   ${configurationmanager_xml}
   configurationmanager_dbus_interface)

# call manager interface
SET ( callmanager_xml  ${dbus_xml_introspecs_path}/callmanager-introspec.xml )

SET_SOURCE_FILES_PROPERTIES(
   ${callmanager_xml}
   PROPERTIES 
   CLASSNAME CallManagerInterface
   INCLUDE "dbus/metatypes.h") 

QT4_ADD_DBUS_INTERFACE(
   sflphone_client_kde_SRCS 
   ${callmanager_xml}
   callmanager_dbus_interface)

# instance interface
SET ( instance_xml  ${dbus_xml_introspecs_path}/instance-introspec.xml )

SET_SOURCE_FILES_PROPERTIES(
   ${instance_xml}
   PROPERTIES 
   CLASSNAME InstanceInterface
   INCLUDE "dbus/metatypes.h") 
   
QT4_ADD_DBUS_INTERFACE(
   sflphone_client_kde_SRCS 
   ${instance_xml}
   instance_dbus_interface)


# kde4_automoc(${sflphone_client_kde_SRCS})

KDE4_ADD_UI_FILES(sflphone_client_kde_SRCS ui/sflphone_kdeview_base.ui ui/ConfigDialog.ui conf/dlggeneralbase.ui)

KDE4_ADD_KCFG_FILES(sflphone_client_kde_SRCS kcfg/settings.kcfgc)
INSTALL(FILES kcfg/sflphone-client-kde.kcfg DESTINATION ${KCFG_INSTALL_DIR})


KDE4_ADD_EXECUTABLE(sflphone-client-kde ${sflphone_client_kde_SRCS} ${QtApp_RCC_SRCS})

TARGET_LINK_LIBRARIES(sflphone-client-kde ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${KDE4_KABC_LIBS})


########### install files ###############

INSTALL(TARGETS sflphone-client-kde DESTINATION ${BIN_INSTALL_DIR})


