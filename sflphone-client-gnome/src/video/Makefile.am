include ../../globals.mak

noinst_LTLIBRARIES = libvideo.la

libvideo_la_SOURCES = video_preview.c

libvideo_la_LDFLAGS = @CLUTTER_LDFLAGS@

libvideo_la_LIBADD = @CLUTTER_LIBS@ \
			@SFLGTK_LIBS@ @SFLGLIB_LIBS@ 
					  
libvideo_la_CFLAGS = @CLUTTER_CFLAGS@ \
			@SFLGTK_CFLAGS@ @SFLGLIB_CFLAGS@
