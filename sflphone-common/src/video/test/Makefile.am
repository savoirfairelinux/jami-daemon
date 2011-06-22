TESTS=test_v4l2
check_PROGRAMS=test_v4l2

test_v4l2_SOURCES=test_v4l2.cpp $(top_srcdir)/src/logger.cpp
test_v4l2_LDADD=$(top_builddir)/src/video/libvideo.la

AM_CXXFLAGS=-I$(top_builddir)/src/video -I$(top_builddir)/src
