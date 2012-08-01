include ../../../globals.mak

TESTS=test_video_endpoint test_thread test_v4l2 test_shm
check_PROGRAMS=test_video_endpoint test_video_rtp test_thread test_video_preview test_v4l2 test_shm

test_video_endpoint_SOURCES=test_video_endpoint.cpp test_video_endpoint.h
test_video_endpoint_LDADD=$(top_builddir)/src/libsflphone.la $(top_builddir)/src/video/libvideo.la $(YAML_LIBS)

test_video_rtp_SOURCES=test_video_rtp.cpp
test_video_rtp_LDADD=$(top_builddir)/src/libsflphone.la $(top_builddir)/src/video/libvideo.la $(YAML_LIBS)

test_video_preview_SOURCES=test_video_preview.cpp test_video_preview.h
test_video_preview_LDADD=$(top_builddir)/src/libsflphone.la $(top_builddir)/src/video/libvideo.la $(YAML_LIBS)

test_thread_SOURCES=test_thread.cpp
test_thread_LDADD=@CCRTP_LIBS@

test_v4l2_SOURCES=test_v4l2.cpp $(top_srcdir)/src/logger.cpp
test_v4l2_LDADD=$(top_builddir)/src/libsflphone.la $(top_builddir)/src/video/libvideo.la $(YAML_LIBS)

test_shm_SOURCES=test_shm.cpp shm_src.cpp shm_src.h
test_shm_LDADD=$(top_builddir)/src/libsflphone.la $(top_builddir)/src/video/libvideo.la $(YAML_LIBS)
test_shm_CXXFLAGS=$(AM_CXXFLAGS) -std=c++0x

AM_CXXFLAGS=-I$(top_builddir)/src/video -I$(top_builddir)/src
