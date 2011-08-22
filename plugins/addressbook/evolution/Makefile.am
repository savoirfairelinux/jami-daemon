include ../../globals.mak

noinst_PROGRAMS = libevladdrbook.so

libevladdrbook_so_SOURCES = eds.c eds.h addressbook.c addressbook.h

libevladdrbook_so_LDFLAGS = --shared $(SFLGLIB_LDFLAGS) $(SFLGTK_LDFLAGS) $(LIBEDATASERVER_LDFLAGS) $(LIBEDATASERVERUI_LDFLAGS) $(LIBEBOOK_LDFLAGS)

libevladdrbook_so_LDADD = $(SFLGLIB_LIBS) $(SFLGTK_LIBS) $(LIBEDATASERVER_LIBS) $(LIBEDATASERVERUI_LIBS) $(LIBEBOOK_LIBS)
	  
libevladdrbook_so_CFLAGS = -fPIC -g -Wall $(SFLGTK_CFLAGS) $(LIBEDATASERVER_CFLAGS) $(LIBEDATASERVERUI_CFLAGS) $(LIBEBOOK_CFLAGS) 

install-exec-local: install-libplugin_libevladdrbook_so

uninstall-local: uninstall-libplugin_libevladdrbook_so

install-libplugin_libevladdrbook_so: libevladdrbook.so
	mkdir -p $(sflplugindir)
	$(INSTALL_PROGRAM) libevladdrbook.so $(sflplugindir)

uninstall-libplugin_libevladdrbook_so:
	rm -f $(sflplugindir)/libevladdrbook.so
	rm -rf $(sflplugindir)
