include $(top_srcdir)/globals.mak

SUBDIRS = decoder rtp

AM_CXXFLAGS = $(GSTREAMER_CFLAGS)

noinst_LTLIBRARIES = libvideo.la

libvideo_la_SOURCES = \
	VideoInputSource.cpp \
	VideoInputSourceGst.cpp  \
	VideoEndpoint.cpp \
	VideoFrame.cpp  \
	FrameFormat.cpp \
	VideoDevice.cpp \
	GstVideoDevice.cpp

libvideo_la_LIBADD = \
	decoder/libvideodecoder.la \
	rtp/libvideortp.la

libvideo_la_LDFLAGS = $(GSTREAMER_LIBS) $(LIBS)

noinst_HEADERS = \
	VideoInputSource.h \
	VideoInputSourceGst.h \
	VideoEndpoint.h \
	FrameFormat.h \
	VideoDevice.h \
	VideoFrame.h \
	FrameFormat.h \
	VideoDevice.h \
	GstVideoDevice.h
					

