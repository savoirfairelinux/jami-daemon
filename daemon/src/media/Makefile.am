include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libmedia.la

SUBDIRS = audio

if RING_VIDEO
SUBDIRS += video
endif

libmedia_la_SOURCES = \
        libav_utils.cpp

noinst_HEADERS = \
        libav_utils.h \
        libav_deps.h

libmedia_la_LIBADD = \
	./audio/libaudio.la

if RING_VIDEO
libmedia_la_libADD = \
    ./video/libvideo.la
endif

libmedia_la_LDFLAGS = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ @LIBAVUTIL_LIBS@

AM_CFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@

AM_CXXFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@

if RING_VIDEO
AM_CXXFLAGS += @UDEV_CFLAGS@
endif
