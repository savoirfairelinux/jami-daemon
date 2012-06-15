CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(sflphone-client-kde)

SET(LOCAL_CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/" )
SET(CMAKE_MODULE_PATH       "${LOCAL_CMAKE_MODULE_PATH}" )

SET(QT_USE_QT*)

# --- custom targets: ---
INCLUDE( ${LOCAL_CMAKE_MODULE_PATH}/TargetDistclean.cmake  REQUIRED)

FIND_PACKAGE ( KdepimLibs REQUIRED            )
FIND_PACKAGE ( KDE4       REQUIRED            )
FIND_PACKAGE ( Qt4        REQUIRED QtCore QtGui QtXml QtDBus QtTest QtSVG QT_USE_QT* )

if (CMAKE_COMPILER_IS_GNUCC)
   execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
   if (GCC_VERSION VERSION_GREATER 4.6 OR GCC_VERSION VERSION_EQUAL 4.6)
      message(STATUS "Found GCC version >= 4.6")
   else()
      message(FATAL_ERROR "Your version of GCC is too old, please install GCC 4.6 or later")
   endif()
endif()


set(AKONADI_MIN_VERSION 1.0)
find_package(Akonadi QUIET NO_MODULE ${AKONADI_MIN_VERSION})

INCLUDE ( KDE4Defaults )

MESSAGE("cmake install prefix is : ${CMAKE_INSTALL_PREFIX}")

INCLUDE_DIRECTORIES ( ${KDE4_INCLUDES} ${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

INCLUDE (  ${CMAKE_ROOT}/Modules/CheckIncludeFiles.cmake  )
INCLUDE(   ${QT_USE_FILE}   )


add_subdirectory( data   )
add_subdirectory( src    )
add_subdirectory( doc    )
add_subdirectory( man    )
add_subdirectory( po     )
add_subdirectory( plasma )

set(PROJECT_VERSION "1.1.0")
set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION})

add_custom_target(dist
      COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
      | gzip > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
