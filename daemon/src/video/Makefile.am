include $(top_srcdir)/globals.mak

SUBDIRS=test

noinst_LTLIBRARIES = libvideo.la
libvideo_la_SOURCES = libav_utils.cpp libav_utils.h video_rtp_session.cpp \
                      video_rtp_session.h video_send_thread.h video_send_thread.cpp \
                      video_receive_thread.h video_receive_thread.cpp \
                      video_preview.h video_preview.cpp video_v4l2.cpp \
                      video_v4l2_list.cpp video_v4l2.h video_v4l2_list.h \
                      video_preferences.h video_preferences.cpp \
                      packet_handle.h packet_handle.cpp check.h shm_header.h \
                      shm_sink.cpp shm_sink.h video_provider.h

libvideo_la_LIBADD = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ @LIBAVUTIL_LIBS@ @CCRTP_LIBS@ @UDEV_LIBS@

AM_CXXFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@ @CCRTP_CFLAGS@ @UDEV_CFLAGS@
AM_CFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@
