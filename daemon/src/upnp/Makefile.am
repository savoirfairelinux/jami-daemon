include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libupnpcontrol.la

libupnpcontrol_la_CXXFLAGS = \
				@CXXFLAGS@ \
				@LIBUPNP_CFLAGS@

libupnpcontrol_la_LDFLAGS = @LIBUPNP_LIBS@

libupnpcontrol_la_SOURCES = \
        upnp_control.cpp \
        upnp_control.h \
        upnp_context.cpp \
        upnp_context.h
