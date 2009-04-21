include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libdbus.la

BUILD_SOURCES=                      \
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
    dbusmanagerimpl.cpp

libdbus_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\"

libdbus_la_LIBADD = $(top_srcdir)/libs/dbus-c++/src/libdbus-c++-1.la

noinst_HEADERS =            \
    callmanager.h           \
    configurationmanager.h  \
    instance.h              \
    dbusmanager.h           \
    dbusmanagerimpl.h       \
    $(BUILD_SOURCES)

# Dbus service file
servicedir = $(DBUS_SERVICES_DIR)
service_in_files = org.sflphone.SFLphone.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	sed -e "s|bindir|$(prefix)/bin|" $<> $@

EXTRA_DIST = *.xml README

CLEANFILES= \
    $(BUILD_SOURCES)
