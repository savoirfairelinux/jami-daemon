SUBDIRS = VideoCodec V4L

noinst_LTLIBRARIES = libvideo.la

libvideo_la_SOURCES =  	VideoCodecDescriptor.cpp \
			VideoCodecDescriptor.cpp \
			VideoRtp.cpp \
			VideoRtpRTX.cpp

AM_CXXFLAGS = -I$(top_srcdir)/include $(libccext2_CFLAGS) $(LIBAVCODEC_CFLAGS) "/usr/include/ffmpeg" $(USER_INCLUDES)
