include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libv4l2.la

libv4l2_la_SOURCES = \
	video_device_impl.cpp \
	video_device_monitor_impl.cpp

if RING_ACCEL
libv4l2_la_SOURCES += vaapi.h vaapi.cpp
endif

AM_CXXFLAGS = @LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@
AM_CXXFLAGS += @UDEV_CFLAGS@ @LIBVA_CFLAGS@

libv4l2_la_LIBADD = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ @LIBAVUTIL_LIBS@
libv4l2_la_LIBADD += @UDEV_LIBS@ @LIBVA_LIBS@
