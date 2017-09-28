include $(top_srcdir)/globals.mk

AM_CPPFLAGS += -I../
noinst_LTLIBRARIES = libdevcrypto.la
libdevcrypto_la_CPPFLAGS = @CPPFLAGS@ -I$(top_srcdir)/src -I$(top_srcdir)/src/ringdht/eth
libdevcrypto_la_SOURCES = Common.cpp
noinst_HEADERS = Common.h
