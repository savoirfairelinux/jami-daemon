include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libclient_dbus.la

BUILT_SOURCES=                      \
    dbuscallmanager.adaptor.h       \
    dbusconfigurationmanager.adaptor.h \
    dbusinstance.adaptor.h

BUILT_SOURCES+=dbuspresencemanager.adaptor.h
dbuspresencemanager.adaptor.h: cx.ring.Ring.PresenceManager.xml Makefile.am
	$(DBUSXX_XML2CPP) $< --adaptor=$@

if ENABLE_VIDEO
BUILT_SOURCES+=dbusvideomanager.adaptor.h
dbusvideomanager.adaptor.h: cx.ring.Ring.VideoManager.xml Makefile.am
	$(DBUSXX_XML2CPP) $< --adaptor=$@
endif

# Rule to generate the binding headers
dbuscallmanager.adaptor.h: cx.ring.Ring.CallManager.xml Makefile.am
	$(DBUSXX_XML2CPP) $< --adaptor=$@

# Rule to generate the binding headers
dbusconfigurationmanager.adaptor.h: cx.ring.Ring.ConfigurationManager.xml Makefile.am
	$(DBUSXX_XML2CPP) $< --adaptor=$@

# Rule to generate the binding headers
dbusinstance.adaptor.h: cx.ring.Ring.Instance.xml Makefile.am
	$(DBUSXX_XML2CPP) $< --adaptor=$@

if ENABLE_PLUGIN
BUILT_SOURCES+=dbuspluginmanagerinterface.adaptor.h
# Rule to generate the binding headers
dbuspluginmanagerinterface.adaptor.h: cx.ring.Ring.PluginManagerInterface.xml Makefile.am
	$(DBUSXX_XML2CPP) $< --adaptor=$@
endif

libclient_dbus_la_SOURCES = \
    dbuscallmanager.cpp \
    dbuscallmanager.h \
    dbusconfigurationmanager.cpp  \
    dbusconfigurationmanager.h \
    dbusinstance.cpp  \
    dbusclient.cpp \
    dbusclient.h \
    dbusinstance.h \
    dbus_cpp.h \
    dbuspresencemanager.cpp \
    dbuspresencemanager.h \
    $(BUILT_SOURCES)

if ENABLE_PLUGIN
libclient_dbus_la_SOURCES += dbuspluginmanagerinterface.cpp dbuspluginmanagerinterface.h
endif

if ENABLE_VIDEO
libclient_dbus_la_SOURCES += dbusvideomanager.cpp dbusvideomanager.h
endif

libclient_dbus_la_CXXFLAGS = -I../ \
            -I$(top_srcdir)/src \
            -I$(top_srcdir)/src/dring \
            -DPREFIX=\"$(prefix)\" \
            -DPROGSHAREDIR=\"${datadir}/ring\" \
            $(DBUSCPP_CFLAGS)

do_subst = $(SED) \
	-e 's,[@]bindir[@],$(bindir),g'

cx.ring.Ring.service: cx.ring.Ring.service.in Makefile
	$(do_subst) < $< > $@

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_DATA = cx.ring.Ring.service

# Dbus interface files
# https://dbus.freedesktop.org/doc/dbus-api-design.html#interface-files
interfacedir = $(datadir)/dbus-1/interfaces
interface_DATA = cx.ring.Ring.CallManager.xml \
                 cx.ring.Ring.ConfigurationManager.xml \
                 cx.ring.Ring.Instance.xml \
                 cx.ring.Ring.PresenceManager.xml 

if ENABLE_PLUGIN
interface_DATA += cx.ring.Ring.PluginManagerInterface.xml
endif

if ENABLE_VIDEO
interface_DATA += cx.ring.Ring.VideoManager.xml
endif

CLEANFILES= \
    $(BUILT_SOURCES) \
	cx.ring.Ring.service
