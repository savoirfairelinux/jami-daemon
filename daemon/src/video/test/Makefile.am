include ../../../globals.mak

TESTS=test_video_endpoint test_shm test_video_input test_video_rtp
check_PROGRAMS=test_video_endpoint test_video_rtp test_video_input test_shm

test_video_endpoint_SOURCES=test_video_endpoint.cpp test_video_endpoint.h
test_video_endpoint_LDADD=$(top_builddir)/src/libsflphone.la $(top_builddir)/src/video/libvideo.la $(YAML_LIBS)

test_video_rtp_SOURCES=test_video_rtp.cpp
test_video_rtp_LDADD=$(top_builddir)/src/libsflphone.la $(top_builddir)/src/video/libvideo.la $(YAML_LIBS)

test_video_input_SOURCES=test_video_input.cpp test_video_input.h
test_video_input_LDADD=$(top_builddir)/src/libsflphone.la $(top_builddir)/src/video/libvideo.la $(YAML_LIBS)

test_shm_SOURCES=test_shm.cpp shm_src.cpp shm_src.h
test_shm_LDADD=$(top_builddir)/src/libsflphone.la $(top_builddir)/src/video/libvideo.la $(YAML_LIBS) -lrt
test_shm_CXXFLAGS=$(AM_CXXFLAGS)

AM_CXXFLAGS=-I$(top_builddir)/src/video -I$(top_builddir)/src
