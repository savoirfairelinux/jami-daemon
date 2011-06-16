SUBDIRS=test

noinst_PROGRAMS = ffmpeg2rtp ffmpeg2shm
noinst_LTLIBRARIES = libvideo_endpoint.la libvideo_v4l2.la

libvideo_endpoint_la_SOURCES = video_endpoint.cpp video_endpoint.h \
							   libav_utils.cpp libav_utils.h \
							   video_rtp_session.cpp video_rtp_session.h \
							   video_rtp_factory.cpp video_rtp_factory.h \
							   video_rtp_thread.h video_rtp_thread.cpp
libvideo_endpoint_la_LIBADD = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ @CCRTP_LIBS@

libvideo_v4l2_la_SOURCES = video_v4l2.cpp video_v4l2.h video_v4l2_list.h video_v4l2_list.cpp

AM_CXXFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@ @CCRTP_CFLAGS@
AM_CFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@
ffmpeg2rtp_SOURCES=ffmpeg2rtp.cpp -I .
ffmpeg2rtp_LDADD=@LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ libvideo_endpoint.la @CCRTP_LIBS@

ffmpeg2shm_SOURCES=ffmpeg2shm.c
ffmpeg2shm_LDADD=@LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@
