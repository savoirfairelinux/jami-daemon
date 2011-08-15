
add_subdirectory(icons)

INSTALL( FILES sflphone-client-kde.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )
INSTALL( FILES sflphone-client-kdeui.rc  DESTINATION  ${DATA_INSTALL_DIR}/sflphone-client-kde )
INSTALL( FILES sflphone-client-kde.notifyrc  DESTINATION ${DATA_INSTALL_DIR}/sflphone-client-kde)

