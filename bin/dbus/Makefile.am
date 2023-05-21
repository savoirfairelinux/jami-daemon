include $(top_srcdir)/globals.mk

libexec_PROGRAMS= jamid

BUILT_SOURCES=                      \
    dbuscallmanager.adaptor.h       \
    dbusconfigurationmanager.adaptor.h \
    dbusinstance.adaptor.h \
    dbuspresencemanager.adaptor.h

dbuspresencemanager.adaptor.h: cx.ring.Ring.PresenceManager.xml Makefile.am
	$(SDBUSCPP_XML2CPP) $< --adaptor=$@

# Rule to generate the binding headers
dbuscallmanager.adaptor.h: cx.ring.Ring.CallManager.xml Makefile.am
	$(SDBUSCPP_XML2CPP) $< --adaptor=$@

# Rule to generate the binding headers
dbusconfigurationmanager.adaptor.h: cx.ring.Ring.ConfigurationManager.xml Makefile.am
	$(SDBUSCPP_XML2CPP) $< --adaptor=$@

# Rule to generate the binding headers
dbusinstance.adaptor.h: cx.ring.Ring.Instance.xml Makefile.am
	$(SDBUSCPP_XML2CPP) $< --adaptor=$@

if ENABLE_VIDEO
dbusvideomanager.adaptor.h: cx.ring.Ring.VideoManager.xml Makefile.am
	$(SDBUSCPP_XML2CPP) $< --adaptor=$@
BUILT_SOURCES+= dbusvideomanager.adaptor.h
endif

if ENABLE_PLUGIN
dbuspluginmanagerinterface.adaptor.h: cx.ring.Ring.PluginManagerInterface.xml Makefile.am
	$(SDBUSCPP_XML2CPP) $< --adaptor=$@
BUILT_SOURCES+= dbuspluginmanagerinterface.adaptor.h
endif

jamid_SOURCES= \
    main.cpp \
    dbuscallmanager.hpp \
    dbusconfigurationmanager.hpp \
    dbusinstance.hpp \
    dbuspresencemanager.hpp \
    $(BUILT_SOURCES)

if ENABLE_VIDEO
    jamid_SOURCES+= dbusvideomanager.hpp
endif
if ENABLE_PLUGIN
    jamid_SOURCES+= dbuspluginmanagerinterface.hpp
endif

jamid_CXXFLAGS= ${SDBUSCPP_CFLAGS} -I$(top_srcdir)/src/jami
jamid_LDADD= ${SDBUSCPP_LIBS} $(top_builddir)/src/libjami.la

do_subst = $(SED) \
	-e 's,[@]LIBEXECDIR[@],$(libexecdir),g'

net.jami.daemon.service: net.jami.daemon.service.in Makefile
	$(do_subst) < $< > $@

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_DATA = net.jami.daemon.service

# Dbus interface files
# https://dbus.freedesktop.org/doc/dbus-api-design.html#interface-files
interfacedir = $(datadir)/dbus-1/interfaces
interface_DATA = cx.ring.Ring.CallManager.xml \
                 cx.ring.Ring.ConfigurationManager.xml \
                 cx.ring.Ring.Instance.xml \
                 cx.ring.Ring.PresenceManager.xml

if ENABLE_VIDEO
interface_DATA += cx.ring.Ring.VideoManager.xml
endif

if ENABLE_PLUGIN
interface_DATA += cx.ring.Ring.PluginManagerInterface.xml
endif

CLEANFILES= $(BUILT_SOURCES) net.jami.daemon.service
