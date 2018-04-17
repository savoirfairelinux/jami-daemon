include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libmedia.la

SUBDIRS = audio

if RING_VIDEO
SUBDIRS += video
endif

libmedia_la_SOURCES = \
	libav_utils.cpp \
	socket_pair.cpp \
	media_buffer.cpp \
	media_decoder.cpp \
	media_encoder.cpp \
	media_io_handle.cpp \
	media_codec.cpp \
	system_codec_container.cpp \
	srtp.c \
	recordable.cpp

noinst_HEADERS = \
	rtp_session.h \
	libav_utils.h \
	libav_deps.h \
	socket_pair.h \
	media_buffer.h \
	media_decoder.h \
	media_encoder.h \
	media_io_handle.h \
	media_device.h \
	media_codec.h \
	system_codec_container.h \
	srtp.h \
	recordable.h \
	decoder_finder.h

libmedia_la_LIBADD = \
	./audio/libaudio.la

if RING_VIDEO
libmedia_la_libADD = \
	./video/libvideo.la
endif

libmedia_la_LDFLAGS = @LIBAVDEVICE_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVCODEC_LIBS@ @LIBSWSCALE_LIBS@ @LIBAVUTIL_LIBS@

if HAVE_WIN32
libmedia_la_LDFLAGS += -lws2_32 -lwsock32 -lshlwapi
endif

AM_CFLAGS = @LIBAVDEVICE_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ @LIBSWSCALE_CFLAGS@ @LIBAVUTIL_CFLAGS@

AM_CXXFLAGS = @LIBAVDEVICE_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVCODEC_CFLAGS@ @LIBSWSCALE_CFLAGS@ @LIBAVUTIL_CFLAGS@
