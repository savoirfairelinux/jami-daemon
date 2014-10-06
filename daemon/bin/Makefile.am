libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = sflphoned

sflphoned_CXXFLAGS = -I$(top_srcdir)/src \
                -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
sflphoned_LDADD = $(top_builddir)/src/libsflphone.la

if HAVE_OSX
sflphoned_SOURCES = osxmain.cpp

else
if SFL_DBUS

SUBDIRS=dbus
sflphoned_SOURCES = main.cpp
sflphoned_CXXFLAGS += ${DBUSCPP_CFLAGS}

sflphoned_LDADD += dbus/libclient_dbus.la \
                ${DBUSCPP_LIBS}
endif
endif
