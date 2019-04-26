include $(top_srcdir)/globals.mk

SUBDIRS = eth/libdevcore eth/libdevcrypto

AM_CPPFLAGS += -I$(top_srcdir)/src/jamidht/eth
noinst_LTLIBRARIES = libringacc.la

libringacc_la_CXXFLAGS = @CXXFLAGS@ @JSONCPP_CFLAGS@

libringacc_la_LIBADD = $(DHT_LIBS) \
    ./eth/libdevcore/libdevcore.la \
    ./eth/libdevcrypto/libdevcrypto.la

libringacc_la_SOURCES = \
        jamiaccount.cpp \
        jamiaccount.h \
        sips_transport_ice.cpp \
        sips_transport_ice.h \
        accountarchive.cpp \
        accountarchive.h \
        p2p.cpp \
        p2p.h \
        contact_list.h \
        contact_list.cpp

if RINGNS
libringacc_la_SOURCES += \
        namedirectory.cpp \
        namedirectory.h
endif

nobase_include_HEADERS= jami_contact.h
