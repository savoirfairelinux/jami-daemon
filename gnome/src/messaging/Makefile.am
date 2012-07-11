include ../../globals.mak

noinst_LTLIBRARIES = libmessaging.la

libmessaging_la_SOURCES = message_tab.c message_tab.h

libmessaging_la_LDFLAGS = @CLUTTER_LDFLAGS@ @CLUTTERGTK_LDFLAGS@

libmessaging_la_LIBADD = @CLUTTER_LIBS@ \
			@GTK_LIBS@ @GLIB_LIBS@  @CLUTTERGTK_LIBS@

libmessaging_la_CFLAGS = @CLUTTER_CFLAGS@ \
			@GTK_CFLAGS@ @GLIB_CFLAGS@ @CLUTTERGTK_CFLAGS@ @DBUSGLIB_CFLAGS@
