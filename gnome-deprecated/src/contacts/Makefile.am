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

libcontacts_la_LIBS = $(DBUSGLIB_LIBS) $(LIBNOTIFY_LIBS) \
                      $(GTK_LIBS) $(GLIB_LIBS) \
                      $(GCONF_LDFLAGS) -ldl

libcontacts_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
                        $(GTK_CFLAGS) $(GLIB_CFLAGS) \
                        $(GCONF_CFLAGS)

-include $(top_srcdir)/git.mk
