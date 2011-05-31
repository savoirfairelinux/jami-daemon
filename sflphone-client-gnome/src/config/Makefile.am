include ../../globals.mak

noinst_LTLIBRARIES = libconfig.la

libconfig_la_SOURCES = \
  addressbook-config.c \
  shortcuts-config.c \
  assistant.c \
  preferencesdialog.c \
  accountlistconfigdialog.c \
  zrtpadvanceddialog.c \
  tlsadvanceddialog.c \
  accountconfigdialog.c \
  hooks-config.c \
  audioconf.c \
  utils.c 

libconfig_la_LDFLAGS = $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) \
			$(SFLGTK_LDFLAGS) $(SFLGLIB_LDFLAGS) $(WEBKIT_LDFLAGS) $(LIBEBOOK_LDFLAGS) \
			$(LIBEDATASERVER_LDFLAGS) $(LIBEDATASERVERUI_LDFLAGS) $(LIBGNOMEUI_LDFLAGS) \
			$(GNOMEDOCUTILS_LDFLAGS)

libconfig_la_LIBADD = $(DBUSGLIB_LIBS) $(LIBNOTIFY_LIBSI) \
			$(SFLGTK_LIBS) $(SFLGLIB_LIBS) $(WEBKIT_LIBS) $(LIBEBOOK_LIBS) \
			$(LIBEDATASERVER_LIBS) $(LIBEDATASERVERUI_LIBS) $(LIBGNOMEUI_LIBS) \
			$(GNOMEDOCUTILS_LIBS)
					  
libconfig_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
			$(SFLGTK_CFLAGS) $(SFLGLIB_CFLAGS) $(WEBKIT_CFLAGS) $(LIBEBOOK_CFLAGS) \
			$(LIBEDATASERVER_CFLAGS) $(LIBEDATASERVERUI_CFLAGS) $(LIBGNOMEUI_CFLAGS) \
			$(GNOMEDOCUTILS_CFLAGS)
