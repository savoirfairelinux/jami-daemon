include $(top_srcdir)/globals.mak

if USE_DHT

noinst_LTLIBRARIES = libringacc.la
libringacc_la_CXXFLAGS = @CXXFLAGS@

SUBDIRS = dhtcpp

libringacc_la_LIBADD = \
        dhtcpp/libdhtcpp.la

libringacc_la_SOURCES = \
        ringaccount.cpp \
        ringaccount.h

endif
