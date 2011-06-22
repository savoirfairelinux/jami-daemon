include $(top_srcdir)/globals.mak

SUBDIRS=test

noinst_LTLIBRARIES = libvideo.la

#SUBDIRS = 

libvideo_la_SOURCES = \
    video_v4l2.cpp \
    video_v4l2_list.cpp

AM_CXXFLAGS = @UDEV_CFLAGS@
libvideo_la_LIBADD = @UDEV_LIBS@

noinst_HEADERS = \
    video_v4l2.h \
    video_v4l2_list.h
