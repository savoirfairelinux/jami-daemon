CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(sflphone-client-kde)

SET(LOCAL_CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
SET(CMAKE_MODULE_PATH "${LOCAL_CMAKE_MODULE_PATH}")

# --- custom targets: ---
INCLUDE( ${LOCAL_CMAKE_MODULE_PATH}/TargetDistclean.cmake  REQUIRED)

FIND_PACKAGE ( Kabc REQUIRED )
FIND_PACKAGE ( KdepimLibs REQUIRED )
# FIND_PACKAGE ( PkgConfig REQUIRED )
FIND_PACKAGE ( KDE4 REQUIRED )
FIND_PACKAGE ( Qt4 REQUIRED )

INCLUDE ( KDE4Defaults )

MESSAGE("cmake install prefix is : ${CMAKE_INSTALL_PREFIX}")

INCLUDE_DIRECTORIES ( ${KDE4_INCLUDES} ${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

INCLUDE (  ${CMAKE_ROOT}/Modules/CheckIncludeFiles.cmake  )
INCLUDE(   ${QT_USE_FILE}   )


add_subdirectory(data)
add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(man)
add_subdirectory(po)
add_subdirectory(plasma)

# make dist target, see:
# https://agateau.wordpress.com/2009/08/09/cmake-and-make-dist-the-simple-version/
set(PROJECT_VERSION "1.0.0")
set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION})
    add_custom_target(dist
            COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
            | gzip > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
