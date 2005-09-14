noinst_LTLIBRARIES = libsflphoneguiserver.la

libsflphoneguiserver_la_SOURCES = \
	$(BUILT_SOURCES) \
	guiserver.cpp \
	lex.yy.cc \
	protocol.tab.cc 

libsflphoneguiserver_la_CXXFLAGS = -DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\"
libsflphoneguiserver_la_LIBADD = 

AM_CPPFLAGS = -I$(top_srcdir) $(libccext2_CFLAGS)

