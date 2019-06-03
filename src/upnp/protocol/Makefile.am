include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libupnpprotocol.la

SUBDIRS = 

if BUILD_PUPNP
SUBDIRS += pupnp
endif
if BUILD_NATPMP
SUBDIRS += natpmp
endif

libupnpprotocol_la_CXXFLAGS = \
                @CXXFLAGS@

libupnpprotocol_la_SOURCES = \
        igd.h \
        igd.cpp \
        mapping.h \
        mapping.cpp \
        global_mapping.h \
        upnp_protocol.h

libupnpprotocol_la_LIBADD = 

if BUILD_PUPNP
libupnpprotocol_la_LIBADD += ./pupnp/libpupnp.la
endif

if BUILD_NATPMP
libupnpprotocol_la_LIBADD += ./natpmp/libnatpmp.la
endif