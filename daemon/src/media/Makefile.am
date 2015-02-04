include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libmedia.la

SUBDIRS = audio

if RING_VIDEO
SUBDIRS += video
endif

libmedia_la_SOURCES = \
	libav_utils.cpp \
	socket_pair.cpp \
	media_decoder.cpp \
	media_encoder.cpp \
	media_io_handle.cpp \
	srtp.c

noinst_HEADERS = \
	rtp_session.h \
	libav_utils.h \
	libav_deps.h \
	socket_pair.h \
	media_decoder.h \
	media_encoder.h \
	media_io_handle.h \
	srtp.h

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
