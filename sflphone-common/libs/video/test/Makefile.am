TESTS=test_video_endpoint test_thread
check_PROGRAMS=test_video_endpoint test_video_rtp test_thread

test_video_endpoint_SOURCES=test_video_endpoint.cpp
test_video_endpoint_LDADD=$(top_builddir)/libs/video/libvideo_endpoint.la

test_video_rtp_SOURCES=test_video_rtp.cpp
test_video_rtp_LDADD=$(top_builddir)/libs/video/libvideo_endpoint.la

test_thread_SOURCES=test_thread.cpp
test_thread_LDADD=@CCRTP_LIBS@
AM_CXXFLAGS=-I./ -I../
