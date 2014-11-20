include ../globals.mak

libexecdir=$(libdir)/dht
libexec_PROGRAMS = dhtnode

dhtnode_SOURCES = dhtnode.cpp

dhtnode_CXXFLAGS= @DHTCPP_CFLAGS@

dhtnode_LDFLAGS = @GNUTLS_LIBS@ -lpthread @DHTCPP_LIBS@
