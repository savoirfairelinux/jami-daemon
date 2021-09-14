include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libwinvideo.la

libwinvideo_la_SOURCES = \
	video_device_impl.cpp \
	video_device_monitor_impl.cpp

AM_OBJCXXFLAGS = -std=c++11

libwinvideo_la_LIBADD = -ldxguid -lole32 -luuid -lstrmiids -loleaut32 -lgdi32