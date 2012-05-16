project(plasma-sflphone)

find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   )


set(sflphone_engine_SRCS
     sflphonEngine.cpp
     sflphoneService.cpp
    )


kde4_add_plugin(plasma_engine_sflphone ${sflphone_engine_SRCS})
target_link_libraries(plasma_engine_sflphone
                      qtsflphone
                      ksflphone
                      ${KDE4_KDECORE_LIBS}
                      ${KDE4_PLASMA_LIBS}
                      ${KDEPIMLIBS_AKONADI_KMIME_LIBS}
                      ${KDEPIMLIBS_AKONADI_LIBS}
                      ${KDEPIMLIBS_AKONADI_CONTACT_LIBS} )

install(TARGETS plasma_engine_sflphone
        DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES plasma-engine-sflphone.desktop
        DESTINATION ${SERVICES_INSTALL_DIR})

install(FILES sflphone.operations
        DESTINATION ${DATA_INSTALL_DIR}/plasma/services)

