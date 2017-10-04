include $(top_srcdir)/globals.mk

SUBDIRS = eth/libdevcore eth/libdevcrypto

AM_CPPFLAGS += -I$(top_srcdir)/src/ringdht/eth
noinst_LTLIBRARIES = libringacc.la

libringacc_la_CXXFLAGS = @CXXFLAGS@ @JSONCPP_CFLAGS@

libringacc_la_LIBADD = $(DHT_LIBS) \
    ./eth/libdevcore/libdevcore.la \
    ./eth/libdevcrypto/libdevcrypto.la

libringacc_la_SOURCES = \
        ringaccount.cpp \
        ringaccount.h \
        sips_transport_ice.cpp \
        sips_transport_ice.h \
        accountarchive.cpp \
        accountarchive.h \
        p2p.cpp \
        p2p.h

if RINGNS
libringacc_la_SOURCES += \
        namedirectory.cpp \
        namedirectory.h
endif
