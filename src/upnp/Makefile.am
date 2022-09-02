noinst_LTLIBRARIES += libupnpcontrol.la

libupnpcontrol_la_LIBADD=

libupnpcontrol_la_SOURCES = \
	./upnp/upnp_control.cpp \
	./upnp/upnp_control.h \
	./upnp/upnp_context.cpp \
	./upnp/upnp_context.h

include ./upnp/protocol/Makefile.am

libjami_la_LIBADD += libupnpcontrol.la
