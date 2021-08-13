noinst_LTLIBRARIES += libwinvideo.la

libwinvideo_la_SOURCES = \
	./media/video/winvideo/video_device_impl.cpp \
	./media/video/winvideo/video_device_monitor_impl.cpp

libwinvideo_la_AM_OBJCXXFLAGS = -std=c++11

libwinvideo_la_LIBADD = -ldxguid -lole32 -luuid -lstrmiids -loleaut32 -lgdi32

libvideo_la_LIBADD += libwinvideo.la
