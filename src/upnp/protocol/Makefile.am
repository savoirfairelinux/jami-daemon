include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libupnpprotocol.la

SUBDIRS = pupnp

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
        upnp_protocol.h \
	upnp_protocol.cpp

libupnpprotocol_la_LIBADD = ./pupnp/libpupnp.la

if BUILD_NATPMP
libupnpprotocol_la_LIBADD += ./natpmp/libnatpmp.la
endif