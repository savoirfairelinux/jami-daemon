include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libv4l2.la

libv4l2_la_SOURCES = \
	video_device_impl.cpp \
	video_device_monitor_impl.cpp \
	vaapi.h vaapi.cpp

AM_CXXFLAGS = @UDEV_CFLAGS@ @LIBVA_CFLAGS@ @LIBVA_DRM_CFLAGS@ @LIBVA_X11_CFLAGS@
libv4l2_la_LIBADD = @UDEV_LIBS@ @X11_LIBS@ @LIBVA_LIBS@ @LIBVA_DRM_LIBS@ @LIBVA_X11_LIBS@
