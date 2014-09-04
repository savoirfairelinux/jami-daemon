libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = dhtnode

dhtnode_SOURCES = dhtnode.cpp \
		  $(top_builddir)/src/logger.c

dhtnode_CXXFLAGS=-I$(top_srcdir)/src/dht ${DBUSCPP_CFLAGS} \
				   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

dhtnode_LDFLAGS = @GNUTLS_LIBS@

dhtnode_LDADD = $(top_builddir)/src/dht/dhtcpp/libdhtcpp.la
