SUBDIRS = 

EXTRA_DIST = *.xml README

noinst_LTLIBRARIES = libdbus.la

libdbus_la_SOURCES = \
    callmanager.cpp \
	configurationmanager.cpp  \
	dbusmanagerimpl.cpp

libdbus_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\"
libdbus_la_CPPFLAGS = -I$(top_srcdir)/libs -I$(top_srcdir)/src $(libccext2_CFLAGS) $(libdbuscpp_CFLAGS)
libdbus_la_LIBADD = 

noinst_HEADERS = \
	callmanager-glue.h \
	callmanager.h  \
	configurationmanager.h  \
	dbusmanager.h \
	dbusmanagerimpl.h
	
# Dbus service file
servicedir = $(DBUS_SERVICES_DIR)
service_in_files = org.sflphone.SFLphone.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	sed -e "s|bindir|$(prefix)/libexec|" $<> $@

