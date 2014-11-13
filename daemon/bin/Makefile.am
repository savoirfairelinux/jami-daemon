if HAVE_WIN32
libexec_PROGRAMS = ringcli
ringcli_SOURCES = winmain.cpp
#FIXME This is temporary
ringcli_LDADD = $(SPEEX_LIBS)
endif

if SFL_DBUS
SUBDIRS=dbus

libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = sflphoned

sflphoned_SOURCES = main.cpp

sflphoned_CXXFLAGS=-I$(top_srcdir)/src ${DBUSCPP_CFLAGS} \
				   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

sflphoned_LDADD = dbus/libclient_dbus.la ${DBUSCPP_LIBS} $(top_builddir)/src/libsflphone.la
endif
