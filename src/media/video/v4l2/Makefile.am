noinst_LTLIBRARIES += libv4l2.la

libv4l2_la_SOURCES = \
	./media/video/v4l2/video_device_impl.cpp \
	./media/video/v4l2/video_device_monitor_impl.cpp

libv4l2_la_AM_CXXFLAGS = \
	@LIBAVCODEC_CFLAGS@ \
	@LIBAVFORMAT_CFLAGS@ \
	@LIBAVDEVICE_CFLAGS@ \
	@LIBSWSCALE_CFLAGS@ \
	@LIBAVUTIL_CFLAGS@ \
	@UDEV_CFLAGS@

libv4l2_la_LIBADD = \
	@LIBAVCODEC_LIBS@ \
	@LIBAVFORMAT_LIBS@ \
	@LIBAVDEVICE_LIBS@ \
	@LIBSWSCALE_LIBS@ \
	@LIBAVUTIL_LIBS@ \
	@UDEV_LIBS@

libvideo_la_LIBADD += libv4l2.la
