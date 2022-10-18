noinst_LTLIBRARIES += libupnpcontrol.la

libupnpcontrol_la_LIBADD=

libupnpcontrol_la_SOURCES = \
	./connectivity/upnp/upnp_control.cpp \
	./connectivity/upnp/upnp_control.h \
	./connectivity/upnp/upnp_context.cpp \
	./connectivity/upnp/upnp_context.h

include ./connectivity/upnp/protocol/Makefile.am

libjami_la_LIBADD += libupnpcontrol.la
