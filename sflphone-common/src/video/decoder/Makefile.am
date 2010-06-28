include $(top_srcdir)/globals.mak

AM_CXXFLAGS = $(LIBAVCODEC_CFLAGS) $(LIBSWSCALE_CFLAGS)

noinst_LTLIBRARIES = libvideodecoder.la

libvideodecoder_la_SOURCES = \
		H264Decoder.cpp \
		VideoDecoder.cpp
		
noinst_HEADERS = \
		VideoDecoder.h \
		H264Decoder.h
			
libvideodecoder_la_LDFLAGS = $(LIBAVCODEC_LIBS) $(LIBSWSCALE_LIBS) $(LIBS)

libvideodecoder_la_LIBADD = $(LIBAVCODEC_LIBS) $(LIBSWSCALE_LIBS) $(LIBS)