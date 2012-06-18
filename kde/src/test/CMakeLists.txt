cmake_minimum_required(VERSION 2.8)

FIND_PACKAGE ( KDE4       REQUIRED            )
FIND_PACKAGE ( Qt4        REQUIRED QtCore QtGui QtXml QtDBus QtTest QtSVG QT_USE_QT* )

INCLUDE(   ${QT_USE_FILE}   )
include(KDE4Defaults)

set( dbus_SRCS dbus_test.cpp )

kde4_add_unit_test( dbus
   TESTNAME "DBus_connection"
   ${dbus_SRCS}
)

target_link_libraries( dbus
   ${KDE4_KDECORE_LIBS}
   ${QT_QTTEST_LIBRARY}
   ${KDE4_KDEUI_LIBS}
   ksflphone
   qtsflphone
)
