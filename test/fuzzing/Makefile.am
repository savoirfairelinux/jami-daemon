include $(top_srcdir)/globals.mk

%.cpp: %.ll
	flex --outfile $@ $^

AM_CXXFLAGS += -I$(top_srcdir)/src -I. -include common.h
AM_LDFLAGS += $(top_builddir)/src/libring.la
check_PROGRAMS =

lib_LTLIBRARIES = libfuzz.la

libfuzz_la_SOURCES = lib/utils.cpp lib/supervisor.cpp lib/gnutls.cpp lib/rand.cpp lib/syslog.cpp lib/sip-fmt.cpp lib/sip-parser.cpp
