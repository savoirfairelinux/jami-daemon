
libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = sflphoned

if HAVE_WIN32
sflphoned_SOURCES = winSFLPhone.cpp
sflphoned_LDADD = $(top_builddir)/src/libsflphone.la


sflphoned_CXXFLAGS=-I$(top_srcdir)/src \
				   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
else
SUBDIRS=dbus
sflphoned_SOURCES = main.cpp
sflphoned_LDADD = dbus/libclient_dbus.la ${DBUSCPP_LIBS} $(top_builddir)/src/libsflphone.la

sflphoned_CXXFLAGS=-I$(top_srcdir)/src ${DBUSCPP_CFLAGS} \
				   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
endif
