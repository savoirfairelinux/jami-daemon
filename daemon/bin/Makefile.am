if HAVE_WIN32
libexec_PROGRAMS = ringcli
ringcli_SOURCES = winmain.cpp
#FIXME This is temporary
ringcli_LDADD = $(SPEEX_LIBS)
endif

if HAVE_OSX
libexec_PROGRAMS = ringcli
ringcli_SOURCES = osxmain.cpp
ringcli_CXXFLAGS = -I$(top_srcdir)/src \
                   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
ringcli_LDADD = $(top_builddir)/src/libsflphone.la
endif

if SFL_DBUS
SUBDIRS=dbus

libexec_PROGRAMS = sflphoned

sflphoned_SOURCES = main.cpp

sflphoned_CXXFLAGS=-I$(top_srcdir)/src ${DBUSCPP_CFLAGS} \
				   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

sflphoned_LDADD = dbus/libclient_dbus.la ${DBUSCPP_LIBS} $(top_builddir)/src/libsflphone.la
endif
