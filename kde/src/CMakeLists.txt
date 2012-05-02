
ADD_DEFINITIONS(
	${KDE4_DEFINITIONS}
	${QT_DEFINITIONS}
	-fexceptions
	-DDATA_INSTALL_DIR="\\\"${DATA_INSTALL_DIR}\\\""
	-DSHARE_INSTALL_PREFIX="\\\"${SHARE_INSTALL_PREFIX}\\\""
)

ADD_DEFINITIONS("-std=c++0x")

add_subdirectory(lib)

MESSAGE("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

IF(${CMAKE_BUILD_TYPE} MATCHES Release)
	MESSAGE("NO DEBUG OUTPUT")
	ADD_DEFINITIONS( -DQT_NO_DEBUG_OUTPUT)
ENDIF(${CMAKE_BUILD_TYPE} MATCHES Release)

SET ( KDE4_KABC_LIBS  -lkabc )

SET(	sflphone_client_kde_SRCS
	SFLPhoneView.cpp
	SFLPhone.cpp
        SFLPhoneapplication.cpp
	widgets/SFLPhoneTray.cpp
	main.cpp
	AccountWizard.cpp
	widgets/AccountItemWidget.cpp
	widgets/CallTreeItem.cpp
        widgets/HistoryTreeItem.cpp
	ActionSetAccountFirst.cpp
	conf/ConfigurationDialog.cpp
	conf/dlggeneral.cpp
	conf/dlgdisplay.cpp
	conf/dlgaccounts.cpp
	conf/dlgaudio.cpp
	conf/dlgaddressbook.cpp
	conf/dlghooks.cpp
	conf/ConfigurationSkeleton.cpp
	conf/ConfigAccountList.cpp
	widgets/Dialpad.cpp
	widgets/ContactItemWidget.cpp
	widgets/ContactDock.cpp
	widgets/HistoryDock.cpp
	widgets/BookmarkDock.cpp
	widgets/TranslucentButtons.cpp
   widgets/CategoryDrawer.cpp
   widgets/CategorizedTreeWidget.cpp
	Codec.cpp
	AccountListModel.cpp
	AkonadiBackend.cpp
        CallView.cpp
        AccountView.cpp
)


# generate rules for building source files from the resources
SET(QtApp_RCCS qrc/resources.qrc)
QT4_ADD_RESOURCES(QtApp_RCC_SRCS ${QtApp_RCCS})



# kde4_automoc(${sflphone_client_kde_SRCS})
SET(    config_ui_files
	conf/dlggeneralbase.ui
	conf/dlgdisplaybase.ui
	conf/dlgaccountsbase.ui
	conf/dlgaudiobase.ui
	conf/dlgaddressbookbase.ui
	conf/dlghooksbase.ui
)

KDE4_ADD_UI_FILES(sflphone_client_kde_SRCS ui/SFLPhoneView_base.ui  ${config_ui_files}  )

KDE4_ADD_KCFG_FILES(sflphone_client_kde_SRCS conf/kcfg_settings.kcfgc)
INSTALL(FILES conf/sflphone-client-kde.kcfg DESTINATION ${KCFG_INSTALL_DIR})

KDE4_ADD_EXECUTABLE(sflphone-client-kde ${sflphone_client_kde_SRCS} ${QtApp_RCC_SRCS})

TARGET_LINK_LIBRARIES(sflphone-client-kde qtsflphone ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS} ${KDEPIMLIBS_AKONADI_KMIME_LIBS} ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_AKONADI_CONTACT_LIBS} ${KDE4_PHONON_LIBS} )

########### install files ###############

INSTALL(TARGETS sflphone-client-kde DESTINATION ${BIN_INSTALL_DIR})
INSTALL( FILES icons/transferarraw.png   DESTINATION  ${DATA_INSTALL_DIR}/sflphone-client-kde )
INSTALL( FILES icons/transfertarrow.svg  DESTINATION  ${DATA_INSTALL_DIR}/sflphone-client-kde )
INSTALL( FILES icons/confBlackWhite.svg  DESTINATION  ${DATA_INSTALL_DIR}/sflphone-client-kde )
INSTALL( FILES icons/confBlackWhite.png  DESTINATION  ${DATA_INSTALL_DIR}/sflphone-client-kde )
