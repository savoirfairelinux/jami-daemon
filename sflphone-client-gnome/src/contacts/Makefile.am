include ../../globals.mak

noinst_LTLIBRARIES = libcontacts.la

libcontacts_la_SOURCES = \
  searchbar.c \
  calllist.c \
  calltab.c \
  calltree.c \
  history.c \
  addrbookfactory.c \
  conferencelist.c

libcontacts_la_LDFLAGS = $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) \
				$(SFLGTK_LDFLAGS) $(SFLGLIB_LDFLAGS) $(WEBKIT_LDFLAGS) $(LIBGNOMEUI_LDFLAGS) \
				$(GNOMEDOCUTILS_LDFLAGS) 
					  
libcontacts_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
				$(SFLGTK_CFLAGS) $(SFLGLIB_CFLAGS) $(WEBKIT_CFLAGS) $(LIBGNOMEUI_CFLAGS) \
				$(GNOMEDOCUTILS_CFLAGS)

