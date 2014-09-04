include $(top_srcdir)/globals.mak

if USE_DHT

noinst_LTLIBRARIES = libdht.la
libdht_la_CXXFLAGS = @CXXFLAGS@

SUBDIRS = dhtcpp

libdht_la_LIBADD = \
        dhtcpp/libdhtcpp.la

libdht_la_SOURCES = \
        dhtaccount.cpp \
        dhtaccount.h

endif
