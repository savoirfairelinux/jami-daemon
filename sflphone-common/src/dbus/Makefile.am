include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libdbus.la

BUILT_SOURCES=                      \
    callmanager-glue.h              \
    configurationmanager-glue.h     \
    instance-glue.h

# Rule to generate the binding headers
%-glue.h: %-introspec.xml Makefile.am
	dbusxx-xml2cpp $< --adaptor=$@

libdbus_la_SOURCES = \
    callmanager.cpp \
    configurationmanager.cpp  \
    instance.cpp  \
    dbusmanagerimpl.cpp  \
    networkmanager.cpp

if USE_NETWORKMANAGER
NETWORKMANAGER=-DUSE_NETWORKMANAGER
else
NETWORKMANAGER=
endif

libdbus_la_CXXFLAGS = \
            -DPREFIX=\"$(prefix)\" \
            -DPROGSHAREDIR=\"${datadir}/sflphone\" \
            $(NETWORKMANAGER)

noinst_HEADERS =            \
    callmanager.h           \
    configurationmanager.h  \
    instance.h              \
    dbusmanager.h           \
    dbusmanagerimpl.h       \
    networkmanager_proxy.h  \
    networkmanager.h        \
    $(BUILT_SOURCES)

# Dbus service file
servicedir = $(DBUS_SERVICES_DIR)
service_in_files = org.sflphone.SFLphone.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	sed -e "s|libexec|$(libdir)/sflphone|" $<> $@

EXTRA_DIST = *.xml README

CLEANFILES= \
    $(BUILT_SOURCES)
