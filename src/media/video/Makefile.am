include $(top_srcdir)/globals.mk

if HAVE_LINUX
if HAVE_ANDROID
SUBDIRS= \
	androidvideo
else
SUBDIRS= \
	v4l2
endif
endif

if HAVE_OSX
SUBDIRS= \
    osxvideo
endif

if HAVE_IOS
SUBDIRS= \
    iosvideo
endif

if HAVE_WIN32
SUBDIRS= \
	winvideo
endif

noinst_LTLIBRARIES = libvideo.la
libvideo_la_SOURCES = \
	video_device.h \
	video_device_monitor.cpp video_device_monitor.h \
	video_base.cpp video_base.h \
	video_scaler.cpp video_scaler.h \
	video_mixer.cpp video_mixer.h \
	video_input.cpp video_input.h \
	video_receive_thread.cpp video_receive_thread.h \
	video_sender.cpp video_sender.h \
	video_rtp_session.cpp video_rtp_session.h \
	sinkclient.cpp sinkclient.h \
	filter_transpose.cpp filter_transpose.h

if RING_ACCEL
libvideo_la_SOURCES += accel.cpp accel.h
endif

libvideo_la_LIBADD = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ @LIBAVUTIL_LIBS@

AM_CXXFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@
AM_CFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@

if HAVE_WIN32
AM_CXXFLAGS+=-static
AM_CFLAGS+=-static
endif
