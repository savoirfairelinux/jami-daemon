SUBDIRS=dbus

libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = sflphoned

sflphoned_SOURCES = main.cpp

sflphoned_CXXFLAGS=-I$(top_srcdir)/src ${DBUSCPP_CFLAGS} \
				   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

sflphoned_LDADD = ${DBUSCPP_LIBS} dbus/libclient_dbus.la $(top_builddir)/src/libsflphone.la
