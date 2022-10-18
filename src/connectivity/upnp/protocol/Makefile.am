noinst_LTLIBRARIES += libupnpprotocol.la

libupnpprotocol_la_SOURCES = \
	./connectivity/upnp/protocol/igd.h \
	./connectivity/upnp/protocol/igd.cpp \
	./connectivity/upnp/protocol/mapping.h \
	./connectivity/upnp/protocol/mapping.cpp \
	./connectivity/upnp/protocol/global_mapping.h \
	./connectivity/upnp/protocol/upnp_protocol.h

libupnpprotocol_la_LIBADD =

if BUILD_PUPNP
include ./connectivity/upnp/protocol/pupnp/Makefile.am
endif

if BUILD_NATPMP
include ./connectivity/upnp/protocol/natpmp/Makefile.am
endif

libupnpcontrol_la_LIBADD += libupnpprotocol.la
