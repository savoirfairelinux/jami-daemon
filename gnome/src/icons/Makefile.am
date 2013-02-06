include ../../globals.mak

noinst_LTLIBRARIES = libicons.la

libicons_la_SOURCES = icon_factory.c \
					  icon_factory.h \
					  pixmap_data.h

libicons_la_LDFLAGS = $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) \
					  $(GTK_LDFLAGS) $(GLIB_LDFLAGS)

libicons_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
					 $(GTK_CFLAGS) $(GLIB_CFLAGS)

-include $(top_srcdir)/git.mk
