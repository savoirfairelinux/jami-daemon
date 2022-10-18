noinst_LTLIBRARIES += libsecurity.la

libsecurity_la_CXXFLAGS = -I$(top_srcdir)/src $(AM_CXXFLAGS)

libsecurity_la_SOURCES = \
		./dhtnet/security/tls_session.cpp \
		./dhtnet/security/tls_session.h \
		./dhtnet/security/tlsvalidator.cpp \
		./dhtnet/security/tlsvalidator.h \
		./dhtnet/security/certstore.cpp \
		./dhtnet/security/certstore.h \
		./dhtnet/security/memory.cpp \
		./dhtnet/security/memory.h \
		./dhtnet/security/diffie-hellman.cpp \
		./dhtnet/security/diffie-hellman.h

libjami_la_LIBADD += libsecurity.la
