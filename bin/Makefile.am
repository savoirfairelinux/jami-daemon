if HAVE_WIN32
bin_PROGRAMS = ringcli
ringcli_SOURCES = winmain.cpp
ringcli_CXXFLAGS = -I$(top_srcdir)/src \
                   -I$(top_srcdir)/src/dring \
                   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
ringcli_LDADD = $(top_builddir)/src/libring.la
endif

if HAVE_OSX
bin_PROGRAMS = ringcli
ringcli_SOURCES = osxmain.cpp
ringcli_CXXFLAGS = -I$(top_srcdir)/src \
                   -I$(top_srcdir)/src/dring \
                   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
ringcli_LDADD = $(top_builddir)/src/libring.la
endif

if RING_DBUS
SUBDIRS=dbus

sbin_PROGRAMS = dring

dring_SOURCES = main.cpp

dring_CXXFLAGS= -I$(top_srcdir)/src ${DBUSCPP_CFLAGS} \
                -I$(top_srcdir)/src/dring \
                -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

if HAVE_LINUX
dring_CXXFLAGS += -lva -lva-drm -lva-x11
endif

dring_LDADD = dbus/libclient_dbus.la ${DBUSCPP_LIBS} $(top_builddir)/src/libring.la
endif
