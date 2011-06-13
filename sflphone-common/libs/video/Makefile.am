SUBDIRS=test

noinst_PROGRAMS = ffmpeg2rtp ffmpeg2shm
noinst_LTLIBRARIES = libvideo_endpoint.la

libvideo_endpoint_la_SOURCES = video_endpoint.cpp video_endpoint.h
libvideo_endpoint_la_LIBADD = @LIBAVFORMAT_LIBS@ 

AM_CXXFLAGS=@LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@
ffmpeg2rtp_SOURCES=ffmpeg2rtp.cpp
ffmpeg2rtp_LDADD=@LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@

ffmpeg2shm_SOURCES=ffmpeg2shm.c
ffmpeg2shm_LDADD=@LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@
