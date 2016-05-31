include $(top_srcdir)/globals.mak

SUBDIRS = eth/libdevcore eth/libdevcrypto

AM_CPPFLAGS += -Ieth $(BOOST_CPPFLAGS)
noinst_LTLIBRARIES = libringacc.la

AM_LDFLAGS = $(BOOST_LDFLAGS)

libringacc_la_CXXFLAGS = @CXXFLAGS@

libringacc_la_LIBADD = $(DHT_LIBS) \
    $(BOOST_SYSTEM_LIB) \
    $(BOOST_FILESYSTEM_LIB) \
    $(BOOST_RANDOM_LIB) \
    $(BOOST_THREAD_LIB) \
    ./eth/libdevcore/libdevcore.la \
    ./eth/libdevcrypto/libdevcrypto.la

libringacc_la_SOURCES = \
        ringaccount.cpp \
        ringaccount.h \
        sip_transport_ice.cpp \
        sip_transport_ice.h \
        sips_transport_ice.cpp \
        sips_transport_ice.h
