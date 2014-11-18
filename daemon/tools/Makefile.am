include ../globals.mak

libexecdir=$(libdir)/dht
libexec_PROGRAMS = dhtnode

dhtnode_SOURCES = dhtnode.cpp

dhtnode_CXXFLAGS=-I$(top_srcdir)/src/ringdht/dhtcpp

dhtnode_LDFLAGS = @GNUTLS_LIBS@ -lpthread

dhtnode_LDADD = $(top_builddir)/src/ringdht/dhtcpp/libdhtcpp.la
