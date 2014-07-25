include ../../globals.mak

if SFL_VIDEO

noinst_LTLIBRARIES = libvideo.la

libvideo_la_SOURCES = video_widget.c video_widget.h video_renderer.c video_renderer.h video_callbacks.c video_callbacks.h shm_header.h video_capabilities.c video_capabilities.h totem-aspect-frame.c totem-aspect-frame.h

libvideo_la_LDFLAGS = @CLUTTER_LDFLAGS@ @CLUTTERGTK_LDFLAGS@  -lm

libvideo_la_LIBADD = @CLUTTER_LIBS@ @GTK_LIBS@ @GLIB_LIBS@ \
					 @CLUTTERGTK_LIBS@ -lrt

libvideo_la_CFLAGS = @CLUTTER_CFLAGS@ \
			@GTK_CFLAGS@ @GLIB_CFLAGS@ @CLUTTERGTK_CFLAGS@ @DBUSGLIB_CFLAGS@

endif

-include $(top_srcdir)/git.mk
