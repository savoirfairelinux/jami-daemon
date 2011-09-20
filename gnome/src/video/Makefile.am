include ../../globals.mak

noinst_LTLIBRARIES = libvideo.la

libvideo_la_SOURCES = video_renderer.c

libvideo_la_LDFLAGS = @CLUTTER_LDFLAGS@ @CLUTTERGTK_LDFLAGS@

libvideo_la_LIBADD = @CLUTTER_LIBS@ \
			@GTK_LIBS@ @GLIB_LIBS@  @CLUTTERGTK_LIBS@
					  
libvideo_la_CFLAGS = @CLUTTER_CFLAGS@ \
			@GTK_CFLAGS@ @GLIB_CFLAGS@ @CLUTTERGTK_CFLAGS@ @DBUSGLIB_CFLAGS@
