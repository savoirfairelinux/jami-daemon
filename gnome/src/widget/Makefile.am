include ../../globals.mak

noinst_LTLIBRARIES = libwidget.la

libwidget_la_SOURCES = minidialog.h minidialog.c imwidget.c imwidget.h

libwidget_la_LDFLAGS = $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) \
            $(GTK_LDFLAGS) $(GLIB_LDFLAGS) $(WEBKIT_LDFLAGS)

libwidget_la_LIBADD = $(DBUSGLIB_LIBS) $(LIBNOTIFY_LIBS) \
            $(GTK_LIBS) $(GLIB_LIBS) $(WEBKIT_LIBS)

libwidget_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
            $(GTK_CFLAGS) $(GLIB_CFLAGS) $(WEBKIT_CFLAGS)
