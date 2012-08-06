include $(top_srcdir)/globals.mak

G729_LIB = libcodec_g729.so
libcodec_g729_so_SOURCES = g729.cpp
libcodec_g729_so_CXXFLAGS = -fPIC -g -Wall
libcodec_g729_so_LDFLAGS = -rdynamic --shared -lc -ldl -lstdc++
libcodec_g729_so_LDADD = libcodecdescriptor.la
INSTALL_G729_RULE = install-libcodec_g729_so

install-exec-local: $(INSTALL_G729_RULE)

install-libcodec_g729_so: libcodec_g729.so
	mkdir -p $(sflcodecdir)

uninstall-libcodec_g729_so:
	rm -f $(sflcodecdir)/libcodec_g729.so