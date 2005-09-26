noinst_LTLIBRARIES = libsflphoneguiserver.la

libsflphoneguiserver_la_SOURCES = $(BUILT_SOURCES) 	guiserverimpl.cpp \
					responsemessage.cpp request.cpp requestfactory.cpp argtokenizer.cpp tcpsessionio.cpp \
		requestmanager.cpp sessionio.cpp tcpstreampool.cpp

libsflphoneguiserver_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\"
libsflphoneguiserver_la_LIBADD = 

AM_CPPFLAGS = -I$(top_srcdir) $(libccext2_CFLAGS)

noinst_HEADERS = responsemessage.h request.h requestfactory.h subcall.h \
			argtokenizer.h tcpsessionio.h requestmanager.h guiserver.h guiserverimpl.h sessionio.h \
	tcpsessionio.h tcpstreampool.h
