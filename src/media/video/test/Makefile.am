include $(top_srcdir)/globals.mak

TESTS=test_shm test_video_input test_video_rtp
check_PROGRAMS=test_video_rtp test_video_input test_shm

test_video_rtp_SOURCES=test_video_rtp.cpp
test_video_rtp_LDADD=$(top_builddir)/src/libring.la $(top_builddir)/src/media/video/libvideo.la $(YAML_LIBS)

test_video_input_SOURCES=test_video_input.cpp test_video_input.h
test_video_input_LDADD=$(top_builddir)/src/libring.la $(top_builddir)/src/media/video/libvideo.la $(YAML_LIBS)

if HAVE_LINUX
test_shm_SOURCES=test_shm.cpp shm_src.cpp shm_src.h
test_shm_LDADD=$(top_builddir)/src/libring.la $(top_builddir)/src/media/video/libvideo.la $(YAML_LIBS) -lrt
test_shm_CXXFLAGS=$(AM_CXXFLAGS)
endif

AM_CXXFLAGS=-I$(top_srcdir)/src/media/video -I$(top_srcdir)/src
