include $(top_srcdir)/globals.mak

SUBDIRS=test

check_PROGRAMS = ffmpeg2rtp ffmpeg2shm
noinst_LTLIBRARIES = libvideo.la
libvideo_la_SOURCES = video_endpoint.cpp video_endpoint.h \
							libav_utils.cpp libav_utils.h \
							video_rtp_session.cpp video_rtp_session.h \
							video_send_thread.h video_send_thread.cpp \
							video_receive_thread.h video_receive_thread.cpp \
							video_preview.h video_preview.cpp \
							video_v4l2.cpp video_v4l2_list.cpp \
							video_v4l2.h video_v4l2_list.h

libvideo_la_LIBADD = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ @CCRTP_LIBS@ @UDEV_LIBS@

AM_CXXFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@ @CCRTP_CFLAGS@ @UDEV_CFLAGS@
AM_CFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@
ffmpeg2rtp_SOURCES=ffmpeg2rtp.cpp -I .
ffmpeg2rtp_LDADD=@LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ libvideo.la $(top_builddir)/src/libsflphone.la @CCRTP_LIBS@

ffmpeg2shm_SOURCES=ffmpeg2shm.c
ffmpeg2shm_LDADD=@LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@
