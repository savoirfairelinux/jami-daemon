include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libosxvideo.la

libosxvideo_la_SOURCES = \
	video_device_impl.mm \
	video_device_monitor_impl.mm

if RING_ACCEL
libosxvideo_la_SOURCES += videotoolbox.h videotoolbox.mm
endif

AM_OBJCXXFLAGS = -std=c++11
