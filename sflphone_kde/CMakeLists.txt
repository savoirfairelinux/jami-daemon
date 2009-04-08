project(sflphone_kde)
find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include_directories( ${KDE4_INCLUDES} ${QT_INCLUDES} )
INCLUDE (  ${CMAKE_ROOT}/Modules/FindQt4.cmake  )
INCLUDE(   ${QT_USE_FILE}   )


SET(
	sflphone_kde_SRCS
	SFLPhone.cpp
	ConfigDialog.cpp
	main.cpp
	sflphone_const.h
	Account.cpp
	AccountList.cpp
	Call.cpp
	CallList.cpp
	configurationmanager_interface.cpp
	configurationmanager_interface_singleton.cpp
	callmanager_interface.cpp
	callmanager_interface_singleton.cpp
	instance_interface.cpp
	instance_interface_singleton.cpp
	AccountWizard.cpp
	AccountItemWidget.cpp
	ActionSetAccountFirst.cpp
	Contact.cpp
)
 
SET(QtApp_RCCS resources.qrc)


# generate rules for building source files from the resources
QT4_ADD_RESOURCES(QtApp_RCC_SRCS ${QtApp_RCCS})


#kde4_automoc(${sflphone_kde_SRCS})

kde4_add_ui_files(sflphone_kde_SRCS sflphone-qt.ui ConfigDialog.ui)

kde4_add_kcfg_files(sflphone_kde_SRCS settings.kcfgc )

kde4_add_executable(sflphone_kde ${sflphone_kde_SRCS} ${QtApp_RCC_SRCS})

target_link_libraries(sflphone_kde ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${KDE4_KABCdef_LIBS})

install(TARGETS sflphone_kde DESTINATION ${BIN_INSTALL_DIR} )


########### install files ###############

install( FILES sflphone_kde.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )
install( FILES sflphone_kde.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES sflphone_kdeui.rc  DESTINATION  ${DATA_INSTALL_DIR}/sflphone_kde )


