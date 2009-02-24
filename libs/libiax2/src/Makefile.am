AM_CFLAGS = -Wall -O2
AM_CFLAGS += -g -Wall -Wstrict-prototypes -I .
AM_CFLAGS += -DLIBIAX
AM_CFLAGS += -fsigned-char
# -DDEBUG_SUPPORT (doesn't compile with it)
# -DDEBUG_DEFAULT 
AM_CFLAGS += $(UCFLAGS)


pkgdir = $(libdir)
pkg_LTLIBRARIES=libiax2.la
libiax2_la_SOURCES = iax2-parser.c iax.c md5.c jitterbuf.c
EXTRA_DIST = md5.h frame.h iax-client.h iax2.h iax2-parser.h jitterbuf.h

install-data-local:
	mkdir -p $(DESTDIR)$(includedir)/iax2
	install -m 644 md5.h $(DESTDIR)$(includedir)/iax2
	install -m 644 frame.h $(DESTDIR)$(includedir)/iax2
	install -m 644 iax.h $(DESTDIR)$(includedir)/iax2
	install -m 644 iax2.h $(DESTDIR)$(includedir)/iax2
	install -m 644 iax2-parser.h $(DESTDIR)$(includedir)/iax2
	install -m 644 iax-client.h $(DESTDIR)$(includedir)/iax2

uninstall-local:
	rm -rf $(DESTDIR)$(includedir)/iax2
