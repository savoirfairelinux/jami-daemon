include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient_dbus.la

BUILT_SOURCES=                      \
    dbuscallmanager.adaptor.h       \
    dbusconfigurationmanager.adaptor.h \
    dbusinstance.adaptor.h

BUILT_SOURCES+=dbuspresencemanager.adaptor.h
dbuspresencemanager.adaptor.h: presencemanager-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@

if SFL_VIDEO
BUILT_SOURCES+=dbusvideomanager.adaptor.h
dbusvideomanager.adaptor.h: videomanager-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@
endif

# Rule to generate the binding headers
dbuscallmanager.adaptor.h: callmanager-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@

# Rule to generate the binding headers
dbusconfigurationmanager.adaptor.h: configurationmanager-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@

# Rule to generate the binding headers
dbusinstance.adaptor.h: instance-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@

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

if SFL_VIDEO
libclient_dbus_la_SOURCES += dbusvideomanager.cpp dbusvideomanager.h
endif

libclient_dbus_la_CXXFLAGS = -I../ \
							 -I$(top_srcdir)/src \
            -DPREFIX=\"$(prefix)\" \
            -DPROGSHAREDIR=\"${datadir}/ring\" \
            $(DBUSCPP_CFLAGS)

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_DATA = cx.ring.Ring.service

EXTRA_DIST = callmanager-introspec.xml \
			 configurationmanager-introspec.xml \
			 instance-introspec.xml \
			 presencemanager-introspec.xml

if SFL_VIDEO
EXTRA_DIST += videomanager-introspec.xml
endif

CLEANFILES= \
    $(BUILT_SOURCES) \
	cx.ring.Ring.service
