if HAVE_WIN32
libexec_PROGRAMS = jamid
jamid_SOURCES = winmain.cpp
jamid_CXXFLAGS = -I$(top_srcdir)/src \
                   -I$(top_srcdir)/src/jami \
                   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\" \
		   $(AM_CXXFLAGS)
jamid_LDADD = $(top_builddir)/src/libjami.la
endif

if HAVE_OSX
libexec_PROGRAMS = jamid
jamid_SOURCES = osxmain.cpp
jamid_CXXFLAGS = -I$(top_srcdir)/src \
                   -I$(top_srcdir)/src/jami \
                   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\" \
		   $(AM_CXXFLAGS)
jamid_LDADD = $(top_builddir)/src/libjami.la
endif

SUBDIRS=

if RING_DBUS
SUBDIRS+=dbus

libexec_PROGRAMS = jamid

jamid_SOURCES = main.cpp

jamid_CXXFLAGS= -I$(top_srcdir)/src ${DBUSCPP_CFLAGS} \
                -I$(top_srcdir)/src/jami \
                -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

jamid_LDADD = dbus/libclient_dbus.la ${DBUSCPP_LIBS} $(top_builddir)/src/libjami.la -ldl
endif

if ENABLE_NODEJS
SUBDIRS+=nodejs
endif
