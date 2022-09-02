include $(top_srcdir)/globals.mk

if ENABLE_FUZZING

%.cpp: %.ll
	flex --outfile $@ $^

AM_CXXFLAGS += -I$(top_srcdir)/src -I. -include common.h
AM_LDFLAGS += $(top_builddir)/src/libjami.la
check_PROGRAMS =

lib_LTLIBRARIES = libfuzz.la

libfuzz_la_SOURCES = lib/utils.cpp lib/supervisor.cpp lib/gnutls.cpp lib/rand.cpp lib/syslog.cpp lib/sip-fmt.cpp lib/sip-parser.cpp

check_PROGRAMS += fuzz_spam_channel_creation
fuzz_spam_channel_creation_SOURCES = fuzz-spam-channel-creation.cpp
fuzz_spam_channel_creation_LDADD = libfuzz.la

check_PROGRAMS += fuzz_msgpack
fuzz_msgpack_SOURCES = fuzz-msgpack.cpp
fuzz_msgpack_LDADD = libfuzz.la
endif
