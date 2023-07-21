noinst_LTLIBRARIES += libsecurity.la

libsecurity_la_CXXFLAGS = -I$(top_srcdir)/src $(AM_CXXFLAGS)

libsecurity_la_SOURCES = \
		./connectivity/security/tlsvalidator.cpp \
		./connectivity/security/tlsvalidator.h \
		./connectivity/security/memory.cpp \
		./connectivity/security/memory.h

libjami_la_LIBADD += libsecurity.la
