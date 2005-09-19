noinst_LTLIBRARIES = libsflphoneguiserver.la

libsflphoneguiserver_la_SOURCES = $(BUILT_SOURCES) 	guiserver.cpp \
	responsemessage.cpp

libsflphoneguiserver_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\"
libsflphoneguiserver_la_LIBADD = 

AM_CPPFLAGS = -I$(top_srcdir) $(libccext2_CFLAGS)

noinst_HEADERS = responsemessage.h
