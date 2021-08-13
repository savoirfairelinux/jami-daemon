noinst_LTLIBRARIES += libupnpprotocol.la

libupnpprotocol_la_SOURCES = \
	./upnp/protocol/igd.h \
	./upnp/protocol/igd.cpp \
	./upnp/protocol/mapping.h \
	./upnp/protocol/mapping.cpp \
	./upnp/protocol/global_mapping.h \
	./upnp/protocol/upnp_protocol.h

libupnpprotocol_la_LIBADD =

if BUILD_PUPNP
include ./upnp/protocol/pupnp/Makefile.am
endif

if BUILD_NATPMP
include ./upnp/protocol/natpmp/Makefile.am
endif

libupnpcontrol_la_LIBADD += libupnpprotocol.la
