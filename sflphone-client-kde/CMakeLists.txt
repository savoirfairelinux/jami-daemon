CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(sflphone-client-kde)

SET(LOCAL_CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
SET(CMAKE_MODULE_PATH "${LOCAL_CMAKE_MODULE_PATH}")

# --- custom targets: ---
INCLUDE( ${LOCAL_CMAKE_MODULE_PATH}/TargetDistclean.cmake  REQUIRED)

FIND_PACKAGE ( Kabc REQUIRED )
FIND_PACKAGE ( PkgConfig REQUIRED )
FIND_PACKAGE ( KDE4 REQUIRED )
FIND_PACKAGE ( Qt4 REQUIRED )

INCLUDE ( KDE4Defaults )



INCLUDE_DIRECTORIES ( ${KDE4_INCLUDES} ${QT_INCLUDES} )

INCLUDE (  ${CMAKE_ROOT}/Modules/CheckIncludeFiles.cmake  )
INCLUDE(   ${QT_USE_FILE}   )

ADD_DEFINITIONS(${KDE4_DEFINITIONS} ${QT_DEFINITIONS} -fexceptions -DDATA_INSTALL_DIR="\\\"${DATA_INSTALL_DIR}\\\"")



#pkg_check_modules(LIBSEXY libsexy>=0.1)
#pkg_check_modules(KDEPIM kdepimlibs5-dev REQUIRED)




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
)
 

# generate rules for building source files from the resources
 
SET(QtApp_RCCS resources.qrc)

QT4_ADD_RESOURCES(QtApp_RCC_SRCS ${QtApp_RCCS})


# kde4_automoc(${sflphone_client_kde_SRCS})

KDE4_ADD_UI_FILES(sflphone_client_kde_SRCS sflphone_kdeview_base.ui ConfigDialog.ui)

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


# kde4_add_kcfg_files(sflphone_client_kde_SRCS settings.kcfgc )

KDE4_ADD_EXECUTABLE(sflphone-client-kde ${sflphone_client_kde_SRCS} ${QtApp_RCC_SRCS})

SET ( KDE4_KABC_LIBS  -lkabc )
TARGET_LINK_LIBRARIES(sflphone-client-kde ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${KDE4_KABC_LIBS})

INSTALL(TARGETS sflphone-client-kde DESTINATION ${BIN_INSTALL_DIR})


########### install files ###############

INSTALL( FILES sflphone-client-kde.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )
# INSTALL( FILES sflphone_kde.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
INSTALL( FILES sflphone-client-kdeui.rc  DESTINATION  ${DATA_INSTALL_DIR}/sflphone-client-kde )


