project(sflphone_kde)
find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include_directories( ${KDE4_INCLUDES} ${QT_INCLUDES} )

set(sflphone_kde_SRCS
	SFLPhone.cpp
   ConfigDialog.cpp
   main.cpp
   sflphone_const.cpp
   Account.cpp
   AccountList.cpp	
   configurationmanager_interface.cpp
   configurationmanager_interface_singleton.cpp
   callmanager_interface.cpp
   callmanager_interface_singleton.cpp
 )

#kde4_automoc(${sflphone_kde_SRCS})

kde4_add_ui_files(sflphone_kde_SRCS sflphone-qt.ui ConfigDialog.ui)

kde4_add_kcfg_files(sflphone_kde_SRCS settings.kcfgc )

kde4_add_executable(sflphone_kde ${sflphone_kde_SRCS})

target_link_libraries(sflphone_kde ${KDE4_KDEUI_LIBS} )

install(TARGETS sflphone_kde DESTINATION ${BIN_INSTALL_DIR} )


########### install files ###############

install( FILES sflphone_kde.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )
install( FILES sflphone_kde.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES sflphone_kdeui.rc  DESTINATION  ${DATA_INSTALL_DIR}/sflphone_kde )


