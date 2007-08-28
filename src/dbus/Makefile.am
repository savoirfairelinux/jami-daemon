SUBDIRS = 

EXTRA_DIST = *.xml README

noinst_LTLIBRARIES = libdbus.la

libdbus_la_SOURCES = \
    callmanager.cpp \
	dbusmanagerimpl.cpp

libdbus_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\"
libdbus_la_CPPFLAGS = -I$(top_srcdir)/libs -I$(top_srcdir)/src $(libccext2_CFLAGS) $(libdbuscpp_CFLAGS)
libdbus_la_LIBADD = 

noinst_HEADERS = \
	callmanager-glue.h \
	callmanager.h  \
	dbusmanager.h \
	dbusmanagerimpl.h
