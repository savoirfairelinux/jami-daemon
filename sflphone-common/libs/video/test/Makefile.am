TESTS=test_video_endpoint test_video_rtp
check_PROGRAMS=test_video_endpoint test_video_rtp

test_video_endpoint_SOURCES=test_video_endpoint.cpp
test_video_endpoint_LDADD=$(top_builddir)/libs/video/libvideo_endpoint.la

test_video_rtp_SOURCES=test_video_rtp.cpp
test_video_rtp_LDADD=$(top_builddir)/libs/video/libvideo_endpoint.la

AM_CXXFLAGS=-I./ -I../
