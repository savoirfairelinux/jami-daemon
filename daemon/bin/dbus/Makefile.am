include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient_dbus.la

BUILT_SOURCES=                      \
    dbuscallmanager.adaptor.h       \
    dbusconfigurationmanager.adaptor.h \
    dbusinstance.adaptor.h

if SFL_PRESENCE
BUILT_SOURCES+=dbuspresencemanager.adaptor.h
dbuspresencemanager.adaptor.h: presencemanager-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@
endif

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
	$(BUILT_SOURCES)

if SFL_PRESENCE
libclient_dbus_la_SOURCES += dbuspresencemanager.cpp dbuspresencemanager.h
endif

if SFL_VIDEO
libclient_dbus_la_SOURCES += dbusvideomanager.cpp dbusvideomanager.h
endif

libclient_dbus_la_CXXFLAGS = -I../ \
							 -I$(top_srcdir)/src \
            -DPREFIX=\"$(prefix)\" \
            -DPROGSHAREDIR=\"${datadir}/sflphone\" \
            $(DBUSCPP_CFLAGS)

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.sflphone.SFLphone.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	sed -e "s|libexec|$(libdir)/sflphone|" $<> $@

EXTRA_DIST = $(service_in_files) \
			 callmanager-introspec.xml \
			 configurationmanager-introspec.xml \
			 instance-introspec.xml

if SFL_PRESENCE
EXTRA_DIST += presencemanager-introspec.xml
endif

if SFL_VIDEO
EXTRA_DIST += videomanager-introspec.xml
endif

CLEANFILES= \
    $(BUILT_SOURCES) \
	org.sflphone.SFLphone.service
