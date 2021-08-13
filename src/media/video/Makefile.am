if ENABLE_VIDEO

noinst_LTLIBRARIES += libvideo.la

libvideo_la_LIBADD=

libvideo_la_SOURCES = \
	./media/video/video_device.h \
	./media/video/video_device_monitor.cpp video_device_monitor.h \
	./media/video/video_base.cpp video_base.h \
	./media/video/video_scaler.cpp video_scaler.h \
	./media/video/video_mixer.cpp video_mixer.h \
	./media/video/video_input.cpp video_input.h \
	./media/video/video_receive_thread.cpp video_receive_thread.h \
	./media/video/video_sender.cpp video_sender.h \
	./media/video/video_rtp_session.cpp video_rtp_session.h \
	./media/video/sinkclient.cpp sinkclient.h \
	./media/video/filter_transpose.cpp filter_transpose.h

if RING_ACCEL
libvideo_la_SOURCES += ./media/video/accel.cpp ./media/video/accel.h
endif

libvideo_la_LIBADD += \
	@LIBAVCODEC_LIBS@ \
	@LIBAVFORMAT_LIBS@ \
	@LIBAVDEVICE_LIBS@ \
	@LIBSWSCALE_LIBS@ \
	@LIBAVUTIL_LIBS@

libvideo_la_CFLAGS = \
	@LIBAVCODEC_CFLAGS@ \
	@LIBAVFORMAT_CFLAGS@ \
	@LIBAVDEVICE_CFLAGS@ \
	@LIBSWSCALE_CFLAGS@ \
	$(AM_CFLAGS)

libvideo_la_CXXFLAGS =  \
	@LIBAVCODEC_CFLAGS@ \
	@LIBAVFORMAT_CFLAGS@ \
	@LIBAVDEVICE_CFLAGS@ \
	@LIBSWSCALE_CFLAGS@ \
	$(AM_CXXFLAGS)

if HAVE_WIN32
libvideo_la_CXXFLAGS += -static
libvideo_la_CFLAGS += -static
endif


if HAVE_LINUX
if HAVE_ANDROID
include ./media/video/androidvideo/Makefile.am
else
include ./media/video/v4l2/Makefile.am
endif
endif

if HAVE_OSX
include ./media/video/osxvideo/Makefile.am
endif

if HAVE_IOS
include ./media/video/iosvideo/Makefile.am
endif

if HAVE_WIN32
include ./media/video/winvideo/Makefile.am
endif

libmedia_la_LIBADD += libvideo.la

endif # ENABLE_VIDEO
