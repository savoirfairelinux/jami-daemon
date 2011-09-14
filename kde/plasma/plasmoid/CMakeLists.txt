# Project Needs a name ofcourse
project(sflphone-plasmoid)
 
# Find the required Libaries
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
 
add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   )
 
# We add our source code here
set(tutorial1_SRCS 
   SFLPhonePlasmoid.cpp
   CallItem.cpp
   ConferenceItem.cpp
   DialPage.cpp
   ViewItem.h
   MainWidget.cpp)

install( FILES
   ../../src/icons/refuse.svg
   ../../src/icons/ring.svg
   ../../src/icons/incoming.svg
   ../../src/icons/outgoing.svg
   ../../src/icons/missed.svg
   ../../src/icons/dial.svg
   ../../src/icons/current.svg
   ../../src/icons/busy.svg
   ../../src/icons/hold.svg
   ../../src/icons/fail.svg
   ../../src/icons/transfert.svg
   ../../src/icons/hang_up.svg
   ../../src/icons/call.svg
   ../../src/icons/sflphone.svg
   ../../src/icons/sflphone_notif.svg
DESTINATION ${DATA_INSTALL_DIR}/sflphone-plasmoid/icons/ )

# Now make sure all files get to the right place
kde4_add_plugin(plasma_applet_sflphone ${tutorial1_SRCS})
target_link_libraries(plasma_applet_sflphone
                      qtsflphone
                      ${KDE4_PLASMA_LIBS} ${KDE4_KDEUI_LIBS})
 
install(TARGETS plasma_applet_sflphone
        DESTINATION ${PLUGIN_INSTALL_DIR})
 
install(FILES plasma-applet-sflphone.desktop
        DESTINATION ${SERVICES_INSTALL_DIR})

#set(sflphone-plasmoid_SRCS
#    SFLPhonePlasmoid.cpp)
# 
#kde4_add_plugin(plasma_applet_sflphone ${sflphone-plasmoid_SRCS})
#target_link_libraries(plasma_applet_sflphone ${KDE4_PLASMA_LIBS} ${KDE4_KIO_LIBS} ${KDE4_KDEUI_LIBS})
# 
#install(TARGETS plasma_applet_sflphone DESTINATION ${PLUGIN_INSTALL_DIR})
#install(FILES plasma-applet-sflphone.desktop DESTINATION ${SERVICES_INSTALL_DIR})
