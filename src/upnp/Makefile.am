include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libupnpcontrol.la

SUBDIRS = mapping
SUBDIRS += igd
SUBDIRS += protocol

libupnpcontrol_la_CXXFLAGS = \
				@CXXFLAGS@

libupnpcontrol_la_SOURCES = \
        upnp_control.cpp \
        upnp_control.h \
        upnp_context.cpp \
        upnp_context.h

libupnpcontrol_la_LIBADD = \
	./mapping/libupnpmapping.la \
        ./igd/libupnpigd.la \
        ./protocol/libupnpprotocol.la