EXTRA_DIST = README props-introspect.xml

AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_PROGRAMS = props-server

props_server_SOURCES = props-glue.h props-server.h props-server.cpp
props_server_LDADD = $(top_builddir)/src/libdbus-c++-1.la

props-glue.h: props-introspect.xml
	$(top_builddir)/tools/dbusxx-xml2cpp $^ --adaptor=$@

BUILT_SOURCES = props-glue.h
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

MAINTAINERCLEANFILES = \
	Makefile.in
