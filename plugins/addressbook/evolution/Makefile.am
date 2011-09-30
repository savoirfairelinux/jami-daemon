include ../../globals.mak

noinst_PROGRAMS = libevladdrbook.so

libevladdrbook_so_SOURCES = eds.c eds.h addressbook.c addressbook.h

libevladdrbook_so_LDFLAGS = --shared $(GLIB_LDFLAGS) $(GTK_LDFLAGS) $(LIBEDATASERVER_LDFLAGS) $(LIBEBOOK_LDFLAGS)

libevladdrbook_so_LDADD = $(GLIB_LIBS) $(GTK_LIBS) $(LIBEDATASERVER_LIBS) $(LIBEBOOK_LIBS)

libevladdrbook_so_CFLAGS = -fPIC -g -Wall $(GTK_CFLAGS) $(LIBEDATASERVER_CFLAGS) $(LIBEBOOK_CFLAGS)

install-exec-local: install-libplugin_libevladdrbook_so

uninstall-local: uninstall-libplugin_libevladdrbook_so

install-libplugin_libevladdrbook_so: libevladdrbook.so
	mkdir -p $(sflplugindir)
	$(INSTALL_PROGRAM) libevladdrbook.so $(sflplugindir)

uninstall-libplugin_libevladdrbook_so:
	rm -f $(sflplugindir)/libevladdrbook.so
	rmdir $(sflplugindir)
