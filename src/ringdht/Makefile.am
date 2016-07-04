include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libringacc.la
libringacc_la_CXXFLAGS = @CXXFLAGS@

libringacc_la_LIBADD = $(DHT_LIBS)

libringacc_la_SOURCES = \
        ringaccount.cpp \
        ringaccount.h \
        sip_transport_ice.cpp \
        sip_transport_ice.h \
        sips_transport_ice.cpp \
        sips_transport_ice.h
