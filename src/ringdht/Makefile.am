include $(top_srcdir)/globals.mk

SUBDIRS = eth/libdevcore eth/libdevcrypto

AM_CPPFLAGS += -I$(top_srcdir)/src/ringdht/eth $(BOOST_CPPFLAGS)
noinst_LTLIBRARIES = libringacc.la

AM_LDFLAGS = $(BOOST_LDFLAGS)

libringacc_la_CXXFLAGS = @CXXFLAGS@ @JSONCPP_CFLAGS@

libringacc_la_LIBADD = $(DHT_LIBS) \
    $(BOOST_SYSTEM_LIB) \
    $(BOOST_RANDOM_LIB) \
    ./eth/libdevcore/libdevcore.la \
    ./eth/libdevcrypto/libdevcrypto.la

libringacc_la_SOURCES = \
	contact.h \
	contactsmanager.cpp \
	contactsmanager.h \
        ringaccount.cpp \
        ringaccount.h \
        sip_transport_ice.cpp \
        sip_transport_ice.h \
        sips_transport_ice.cpp \
        sips_transport_ice.h 

if RINGNS
libringacc_la_SOURCES += \
        namedirectory.cpp \
        namedirectory.h
endif
