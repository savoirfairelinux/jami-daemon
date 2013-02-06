include ../../globals.mak

noinst_LTLIBRARIES = libmessaging.la

libmessaging_la_SOURCES = message_tab.c message_tab.h

libmessaging_la_LDFLAGS = $(GTK_LDFLAGS)  $(GLIB_LDFLAGS) $(DBUSGLIB_LDFLAGS)

libmessaging_la_LIBADD = @GTK_LIBS@ @GLIB_LIBS@ $(DBUSGLIB_LIBS)

libmessaging_la_CFLAGS = @GTK_CFLAGS@ @GLIB_CFLAGS@ @DBUSGLIB_CFLAGS@

-include $(top_srcdir)/git.mk
