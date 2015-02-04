include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libv4l2.la

libv4l2_la_SOURCES = \
	video_device_impl.cpp \
	video_device_monitor_impl.cpp

AM_CXXFLAGS = @UDEV_CFLAGS@
libv4l2_la_LIBADD = @UDEV_LIBS@
