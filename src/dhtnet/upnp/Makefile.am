noinst_LTLIBRARIES += libupnpcontrol.la

libupnpcontrol_la_LIBADD=

libupnpcontrol_la_SOURCES = \
	./dhtnet/upnp/upnp_control.cpp \
	./dhtnet/upnp/upnp_control.h \
	./dhtnet/upnp/upnp_context.cpp \
	./dhtnet/upnp/upnp_context.h

include ./dhtnet/upnp/protocol/Makefile.am

libjami_la_LIBADD += libupnpcontrol.la
