# Install script for directory: /home/emmanuel/sflphone/sflphone-client-kde/doc

# Set the install prefix
IF(NOT DEFINED CMAKE_INSTALL_PREFIX)
  SET(CMAKE_INSTALL_PREFIX "/usr/local")
ENDIF(NOT DEFINED CMAKE_INSTALL_PREFIX)
STRING(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
IF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  IF(BUILD_TYPE)
    STRING(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  ELSE(BUILD_TYPE)
    SET(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
  ENDIF(BUILD_TYPE)
  MESSAGE(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
ENDIF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)

# Set the component getting installed.
IF(NOT CMAKE_INSTALL_COMPONENT)
  IF(COMPONENT)
    MESSAGE(STATUS "Install component: \"${COMPONENT}\"")
    SET(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  ELSE(COMPONENT)
    SET(CMAKE_INSTALL_COMPONENT)
  ENDIF(COMPONENT)
ENDIF(NOT CMAKE_INSTALL_COMPONENT)

# Install shared libraries without execute permission?
IF(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  SET(CMAKE_INSTALL_SO_NO_EXE "1")
ENDIF(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  FILE(INSTALL DESTINATION "/usr/local/share/doc/HTML/en/sflphone-client-kde" TYPE FILE FILES
    "/home/emmanuel/sflphone/sflphone-client-kde/cmake-build/doc/index.cache.bz2"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/credits.docbook"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/getting-started.docbook"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/index.docbook"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/advanced-use.docbook"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/common-use.docbook"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/basic-use.docbook"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/introduction.docbook"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/wizard-create-email.png"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/wizard-register-siporiax.png"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/wizard-finish.png"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/wizard-createorregister.png"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/wizard-stun.png"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/wizard-register-settings.png"
    "/home/emmanuel/sflphone/sflphone-client-kde/doc/wizard-welcome.png"
    )
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  execute_process(COMMAND /usr/bin/cmake -E create_symlink "/usr/local/share/doc/HTML/en/common"  "$ENV{DESTDIR}/usr/local/share/doc/HTML/en/sflphone-client-kde/common" )
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")

