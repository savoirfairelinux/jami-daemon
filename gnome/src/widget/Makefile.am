include ../../globals.mak

noinst_LTLIBRARIES = libwidget.la

libwidget_la_SOURCES = \
		gtkscrollbook.c \
		gtkscrollbook.h \
		minidialog.h \
		minidialog.c \
		imwidget.c \
		imwidget.h

libwidget_la_LDFLAGS = $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) \
			$(GTK_LDFLAGS) $(GLIB_LDFLAGS) $(WEBKIT_LDFLAGS) $(LIBGNOMEUI_LDFLAGS) \
			$(GNOMEDOCUTILS_LDFLAGS)

libwidget_la_LIBADD = $(DBUSGLIB_LIBS) $(LIBNOTIFY_LIBS)
			$(GTK_LIBS) $(GLIB_LIBS) $(WEBKIT_LIBS) $(LIBGNOMEUI_LIBS) \
			$(GNOMEDOCUTILS_LIBS)

libwidget_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
			$(GTK_CFLAGS) $(GLIB_CFLAGS) $(WEBKIT_CFLAGS) $(LIBEBOOK_CFLAGS) \
			$(LIBEDATASERVER_CFLAGS) $(LIBEDATASERVERUI_CFLAGS) $(LIBGNOMEUI_CFLAGS) \
			$(GNOMEDOCUTILS_CFLAGS)
