include $(top_srcdir)/globals.mak

if BUILD_UPNP
noinst_LTLIBRARIES = libupnp.la

# libupnp_la_CXXFLAGS = @CXXFLAGS@

libupnp_la_LIBADD = @UPNP_LIBS@

libupnp_la_SOURCES = \
        upnp.c \
        upnp.h

endif
