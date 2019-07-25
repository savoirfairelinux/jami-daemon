ringlibdir = $(libdir)/jami

if HAVE_WIN32
ringlib_PROGRAMS = ringcli
ringcli_SOURCES = winmain.cpp
ringcli_CXXFLAGS = -I$(top_srcdir)/src \
                   -I$(top_srcdir)/src/dring \
                   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
ringcli_LDADD = $(top_builddir)/src/libring.la
endif

if HAVE_OSX
ringlib_PROGRAMS = ringcli
ringcli_SOURCES = osxmain.cpp
ringcli_CXXFLAGS = -I$(top_srcdir)/src \
                   -I$(top_srcdir)/src/dring \
                   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
ringcli_LDADD = $(top_builddir)/src/libring.la
endif

SUBDIRS=

if RING_DBUS
SUBDIRS+=dbus

ringlib_PROGRAMS = jamid.1

jamid_1_SOURCES = main.cpp

jamid_1_CXXFLAGS= -I$(top_srcdir)/src \
                  -I$(top_srcdir)/src/dring \
                  $(DBUSCPP_CFLAGS) $(SIGCPP_CFLAGS) \
                  -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

jamid_1_LDADD = dbus/libclient_dbus.la $(top_builddir)/src/libring.la ${DBUSCPP_LIBS} ${SIGCPP_LIBS} -ldl
endif

if RING_NODEJS
SUBDIRS+=nodejs
endif

if RING_RESTCPP
SUBDIRS+=restcpp

sbin_PROGRAMS = restdring

restdring_SOURCES = main.cpp

restdring_CXXFLAGS= -g \
                -I$(top_srcdir)/src \
                -I$(top_srcdir)/src/dring \
                -DREST_API \
                -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

restdring_LDADD = restcpp/libclient_rest.la $(top_builddir)/src/libring.la
endif
