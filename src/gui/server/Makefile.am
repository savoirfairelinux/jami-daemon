noinst_LTLIBRARIES = libsflphoneguiserver.la

libsflphoneguiserver_la_SOURCES = guiserverimpl.cpp \
		responsemessage.cpp request.cpp requestfactory.cpp argtokenizer.cpp tcpsessionio.cpp \
		requestmanager.cpp sessionio.cpp tcpstreampool.cpp requestconfig.cpp

libsflphoneguiserver_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\"
libsflphoneguiserver_la_CPPFLAGS = -I$(top_srcdir) $(libccext2_CFLAGS)
libsflphoneguiserver_la_LIBADD = 

noinst_HEADERS = responsemessage.h request.h requestfactory.h subcall.h \
		argtokenizer.h tcpsessionio.h requestmanager.h guiserver.h guiserverimpl.h sessionio.h \
		tcpsessionio.h tcpstreampool.h requestconfig.h ObjectPool.hpp ObjectPool.inl
