noinst_LTLIBRARIES += libupnpprotocol.la

libupnpprotocol_la_SOURCES = \
	./dhtnet/upnp/protocol/igd.h \
	./dhtnet/upnp/protocol/igd.cpp \
	./dhtnet/upnp/protocol/mapping.h \
	./dhtnet/upnp/protocol/mapping.cpp \
	./dhtnet/upnp/protocol/global_mapping.h \
	./dhtnet/upnp/protocol/upnp_protocol.h

libupnpprotocol_la_LIBADD =

if BUILD_PUPNP
include ./dhtnet/upnp/protocol/pupnp/Makefile.am
endif

if BUILD_NATPMP
include ./dhtnet/upnp/protocol/natpmp/Makefile.am
endif

libupnpcontrol_la_LIBADD += libupnpprotocol.la
