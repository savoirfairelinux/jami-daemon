sflcodecdir = $(libdir)/sflphone/codecs

noinst_PROGRAMS = libcodec_ulaw.so libcodec_alaw.so libcodec_gsm.so libcodec_speex.so libcodec_ilbc.so

noinst_HEADERS = audiocodec.h

libcodec_ulaw_so_SOURCES = ulaw.cpp
libcodec_ulaw_so_CFLAGS = -fPIC -g -Wall
libcodec_ulaw_so_LDFLAGS = -shared -lc

libcodec_alaw_so_SOURCES = alaw.cpp
libcodec_alaw_so_CFLAGS = -fPIC -g -Wall
libcodec_alaw_so_LDFLAGS = -shared -lc

libcodec_gsm_so_SOURCES = gsmcodec.cpp
libcodec_gsm_so_CFLAGS = -fPIC -g -Wall
libcodec_gsm_so_LDFLAGS = -shared -lc -lgsm

libcodec_speex_so_SOURCES = speexcodec.cpp
libcodec_speex_so_CFLAGS = -fPIC -g -Wall
libcodec_speex_so_LDFLAGS = -shared -lc -lspeex

libcodec_ilbc_so_SOURCES = ilbc.cpp
libcodec_ilbc_so_CFLAGS = -fPIC -g -Wall
libcodec_ilbc_so_LDFLAGS = -shared ilbc/*.o -lc

SUBDIRS = ilbc

install-exec-local:     install-libcodec_ulaw_so install-libcodec_alaw_so install-libcodec_gsm_so install-libcodec_speex_so install-libcodec_ilbc_so
uninstall-local:  uninstall-libcodec_ulaw_so uninstall-libcodec_alaw_so uninstall-libcodec_gsm_so uninstall-libcodec_speex_so uninstall-libcodec_ilbc_so

install-libcodec_ulaw_so: libcodec_ulaw.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_ulaw.so $(sflcodecdir)
install-libcodec_alaw_so: libcodec_alaw.so
	$(INSTALL_PROGRAM)  libcodec_alaw.so $(sflcodecdir)
install-libcodec_gsm_so: libcodec_gsm.so
	$(INSTALL_PROGRAM)  libcodec_gsm.so $(sflcodecdir)
install-libcodec_speex_so: libcodec_speex.so
	$(INSTALL_PROGRAM)  libcodec_speex.so $(sflcodecdir)
install-libcodec_ilbc_so: libcodec_ilbc.so
	$(INSTALL_PROGRAM)  libcodec_ilbc.so $(sflcodecdir)


uninstall-libcodec_ulaw_so:
	rm -f $(sflcodecdir)/libcodec_ulaw.so
uninstall-libcodec_alaw_so:
	rm -f $(sflcodecdir)/libcodec_alaw.so
uninstall-libcodec_gsm_so:
	rm -f $(sflcodecdir)/libcodec_gsm.so
uninstall-libcodec_speex_so:
	rm -f $(sflcodecdir)/libcodec_speex.so
uninstall-libcodec_ilbc_so:
	rm -f $(sflcodecdir)/libcodec_ilbc.so
	rm -rf $(sflcodecdir)

