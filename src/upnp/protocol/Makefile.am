include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libupnpprotocol.la

SUBDIRS = \

if BUILD_PUPNP
        pupnp \
endif

if BUILD_NATPMP
        natpmp
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

libupnpprotocol_la_LIBADD = \ 

if BUILD_PUPNP
        ./pupnp/libpupnp.la \
endif

if BUILD_NATPMP
        ./natpmp/libnatpmp.la 
endif