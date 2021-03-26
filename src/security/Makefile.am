include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libsecurity.la
libsecurity_la_CXXFLAGS = -I$(top_srcdir)/src $(AM_CXXFLAGS)

libsecurity_la_SOURCES = \
		tls_session.cpp \
		tls_session.h \
		tlsvalidator.cpp \
		tlsvalidator.h \
		certstore.cpp \
		certstore.h \
		memory.cpp \
		memory.h \
		diffie-hellman.cpp \
		diffie-hellman.h
