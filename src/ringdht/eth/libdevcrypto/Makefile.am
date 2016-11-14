include $(top_srcdir)/globals.mk

AM_CPPFLAGS += -I../
noinst_LTLIBRARIES = libdevcrypto.la
libdevcrypto_la_CPPFLAGS = @CPPFLAGS@ -I$(top_srcdir)/src -I$(top_srcdir)/src/ringdht/eth -DBOOST_SYSTEM_NO_DEPRECATED
libdevcrypto_la_CXXFLAGS = @CXXFLAGS@

libdevcrypto_la_SOURCES = AES.cpp \
	ECDHE.cpp \
	Common.cpp \
	CryptoPP.cpp
