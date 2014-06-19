include $(top_srcdir)/globals.mak

SUBDIRS=v4l2 test

noinst_LTLIBRARIES = libvideo.la
libvideo_la_SOURCES = \
	video_device.h \
	video_device_monitor.cpp video_device_monitor.h \
	video_base.cpp video_base.h \
	video_scaler.cpp video_scaler.h \
	video_decoder.cpp video_decoder.h \
	video_encoder.cpp video_encoder.h \
	video_mixer.cpp video_mixer.h \
	socket_pair.cpp socket_pair.h \
	shm_sink.cpp shm_sink.h \
	video_input.cpp video_input.h \
	video_receive_thread.cpp video_receive_thread.h \
	video_sender.cpp video_sender.h \
	video_rtp_session.cpp video_rtp_session.h \
	shm_header.h video_provider.h \
	libav_utils.cpp libav_utils.h libav_deps.h

libvideo_la_LIBADD = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ @LIBAVUTIL_LIBS@

AM_CXXFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@ @UDEV_CFLAGS@
AM_CFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@
