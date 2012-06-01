include ../../globals.mak

noinst_LTLIBRARIES = libcontacts.la

libcontacts_la_SOURCES = \
  searchbar.c \
  calllist.c \
  calltab.c \
  calltree.c \
  history.c \
  addrbookfactory.c \
  conferencelist.c \
  addrbookfactory.h \
  addressbook.h \
  calllist.h \
  calltab.h \
  calltree.h \
  conferencelist.h \
  history.h \
  searchbar.h

libcontacts_la_LDFLAGS = $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) \
						 $(GTK_LDFLAGS) $(GLIB_LDFLAGS) $(WEBKIT_LDFLAGS) \
						 $(GCONF_LDFLAGS)

libcontacts_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
						$(GTK_CFLAGS) $(GLIB_CFLAGS) $(WEBKIT_CFLAGS) \
						$(GCONF_CFLAGS)
