include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libsecurity.la
libsecurity_la_CXXFLAGS = @CXXFLAGS@ -I$(top_srcdir)/src

libsecurity_la_SOURCES = \
		tls_session.cpp \
		tls_session.h \
		tlsvalidator.cpp \
		tlsvalidator.h \
		certstore.cpp \
		certstore.h
