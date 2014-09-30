include ../globals.mak

libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = dhtnode

dhtnode_SOURCES = \
		  $(top_builddir)/src/logger.h \
		  $(top_builddir)/src/logger.c \
		  dhtnode.cpp

dhtnode_CXXFLAGS=-I$(top_srcdir)/src/dht -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

dhtnode_LDFLAGS = @GNUTLS_LIBS@

dhtnode_LDADD = $(top_builddir)/src/dht/dhtcpp/libdhtcpp.la
