include ../../globals.mak

noinst_LTLIBRARIES = libicons.la

libicons_la_SOURCES = \
  icon_factory.c

noinst_libicons_la_HEADER= \
	icon_factory.h	\
	pixmaps_data.h

libicons_la_LDFLAGS = $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) \
			$(SFLGTK_LDFLAGS) $(SFLGLIB_LDFLAGS) $(WEBKIT_LDFLAGS) $(LIBGNOMEUI_LDFLAGS) \
			$(GNOMEDOCUTILS_LDFLAGS)
					  
libicons_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
			$(SFLGTK_CFLAGS) $(SFLGLIB_CFLAGS) $(WEBKIT_CFLAGS) $(LIBGNOMEUI_CFLAGS) \
			$(GNOMEDOCUTILS_CFLAGS)
