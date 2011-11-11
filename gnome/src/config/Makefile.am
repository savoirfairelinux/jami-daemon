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
  utils.c \
  accountconfigdialog.h \
  accountlistconfigdialog.h \
  addressbook-config.h \
  assistant.h \
  audioconf.h \
  hooks-config.h \
  preferencesdialog.h \
  shortcuts-config.h \
  tlsadvanceddialog.h \
  utils.h \
  zrtpadvanceddialog.h

libconfig_la_LDFLAGS = $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) $(GCONF_LDFLAGS) \
					   $(GTK_LDFLAGS) $(GLIB_LDFLAGS) $(WEBKIT_LDFLAGS)

libconfig_la_LIBADD = $(DBUSGLIB_LIBS) $(LIBNOTIFY_LIBS) $(GCONF_LIBS) \
					  $(GTK_LIBS) $(GLIB_LIBS) $(WEBKIT_LIBS)

libconfig_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) $(GCONF_CFLAGS) \
					  $(GTK_CFLAGS) $(GLIB_CFLAGS) $(WEBKIT_CFLAGS)
