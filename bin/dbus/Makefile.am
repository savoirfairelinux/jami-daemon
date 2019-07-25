include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libclient_dbus.la

# Rules to generate the binding headers
dbuscallmanager.adaptor.h: net.jami.daemon1.CallManager.xml Makefile.am
	sdbus-c++-xml2cpp $< --adaptor=$@

dbusconfigurationmanager.adaptor.h: net.jami.daemon1.ConfigurationManager.xml Makefile.am
	sdbus-c++-xml2cpp $< --adaptor=$@

dbusdatatransfer.adaptor.h: net.jami.daemon1.DataTransfer.xml Makefile.am
	sdbus-c++-xml2cpp $< --adaptor=$@

dbusinstance.adaptor.h: net.jami.daemon1.Instance.xml Makefile.am
	sdbus-c++-xml2cpp $< --adaptor=$@

dbuspresencemanager.adaptor.h: net.jami.daemon1.PresenceManager.xml Makefile.am
	sdbus-c++-xml2cpp $< --adaptor=$@

if ENABLE_VIDEO
dbusvideomanager.adaptor.h: net.jami.daemon1.VideoManager.xml Makefile.am
	sdbus-c++-xml2cpp $< --adaptor=$@
endif

BUILT_SOURCES=                      \
    dbuscallmanager.adaptor.h       \
    dbusconfigurationmanager.adaptor.h \
    dbusdatatransfer.adaptor.h \
    dbusinstance.adaptor.h \
    dbuspresencemanager.adaptor.h

if ENABLE_VIDEO
    BUILT_SOURCES += dbusvideomanager.adaptor.h
endif

libclient_dbus_la_SOURCES = \
    dbusclient.cpp \
    dbusclient.h \
    dbusdaemon1.hpp \
    $(BUILT_SOURCES)

libclient_dbus_la_CXXFLAGS= \
            -I$(top_srcdir)/src \
            -I$(top_srcdir)/src/dring \
            $(DBUSCPP_CFLAGS) \
            $(SIGCPP_CFLAGS)

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_DATA = net.jami.daemon1.service

# Dbus interface files
# https://dbus.freedesktop.org/doc/dbus-api-design.html#interface-files
interfacedir = $(datadir)/dbus-1/interfaces
interface_DATA = net.jami.daemon1.CallManager.xml \
                 net.jami.daemon1.ConfigurationManager.xml \
                 net.jami.daemon1.DataTransfer.xml \
                 net.jami.daemon1.Instance.xml \
                 net.jami.daemon1.PresenceManager.xml

if ENABLE_VIDEO
    interface_DATA += net.jami.daemon1.VideoManager.xml
endif

CLEANFILES= \
    $(BUILT_SOURCES) \
    net.jami.daemon1.service
