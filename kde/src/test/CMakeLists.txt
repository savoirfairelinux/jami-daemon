cmake_minimum_required(VERSION 2.8)

FIND_PACKAGE ( KDE4       REQUIRED            )
FIND_PACKAGE ( Qt4        REQUIRED QtCore QtGui QtXml QtDBus QtTest QtSVG QT_USE_QT* )

INCLUDE(   ${QT_USE_FILE}   )
include(KDE4Defaults)

#DBUS tests
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

#ACCOUNT tests
set( account_SRCS account_test.cpp )

kde4_add_unit_test( account
   TESTNAME "Account_serialization"
   ${account_SRCS}
)



target_link_libraries( account
   ${KDE4_KDECORE_LIBS}
   ${QT_QTTEST_LIBRARY}
   ${KDE4_KDEUI_LIBS}
   ksflphone
   qtsflphone
)

#CALL tests
set( call_SRCS call_test.cpp )

kde4_add_unit_test( call
   TESTNAME "Basic_calls"
   ${call_SRCS}
)



target_link_libraries( call
   ${KDE4_KDECORE_LIBS}
   ${QT_QTTEST_LIBRARY}
   ${KDE4_KDEUI_LIBS}
   ksflphone
   qtsflphone
)