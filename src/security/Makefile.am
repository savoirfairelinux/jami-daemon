noinst_LTLIBRARIES += libsecurity.la

libsecurity_la_CXXFLAGS = -I$(top_srcdir)/src $(AM_CXXFLAGS)

libsecurity_la_SOURCES = \
		./security/tls_session.cpp \
		./security/tls_session.h \
		./security/tlsvalidator.cpp \
		./security/tlsvalidator.h \
		./security/certstore.cpp \
		./security/certstore.h \
		./security/memory.cpp \
		./security/memory.h \
		./security/diffie-hellman.cpp \
		./security/diffie-hellman.h

libjami_la_LIBADD += libsecurity.la
