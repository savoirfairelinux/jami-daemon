include ../globals.mak

libexecdir=$(libdir)/dht
libexec_PROGRAMS = dhtnode

dhtnode_SOURCES = dhtnode.cpp

dhtnode_CXXFLAGS= @OPENDHT_CFLAGS@

dhtnode_LDFLAGS = @GNUTLS_LIBS@ -lpthread @OPENDHT_LIBS@
