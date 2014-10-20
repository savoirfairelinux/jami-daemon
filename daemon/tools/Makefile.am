include ../globals.mak

libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = dhtnode

dhtnode_SOURCES = \
		  $(top_srcdir)/src/logger.h \
		  $(top_srcdir)/src/logger.c \
		  dhtnode.cpp

dhtnode_CXXFLAGS=-I$(top_srcdir)/src/dht -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

dhtnode_LDFLAGS = @GNUTLS_LIBS@ -lpthread

dhtnode_LDADD = $(top_builddir)/src/dht/dhtcpp/libdhtcpp.la
