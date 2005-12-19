SUBDIRS = 

noinst_LTLIBRARIES = libzeroconf.la

libzeroconf_la_SOURCES = \
	DNSQueryThread.cpp  DNSQueryThread.h \
	DNSService.cpp  DNSService.h  \
	DNSServiceTXTRecord.cpp  DNSServiceTXTRecord.h

AM_CXXFLAGS = $(libccext2_CFLAGS)
libzeroconf_la_LIBADD = $(LIB_DNSSD) 

