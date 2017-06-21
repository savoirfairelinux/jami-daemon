include $(top_srcdir)/globals.mk

AM_CXXFLAGS=-I$(top_srcdir)/src/media/video -I$(top_srcdir)/src
check_PROGRAMS=

#
# base64 testsuite
#
check_PROGRAMS+= base64
base64_SOURCES= base64.cpp
base64_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

#
# video_input testsuite
#
#check_PROGRAMS+= test_video_input
#test_video_input_SOURCES= media/video/test_video_input.cpp media/video/test_video_input.h
#test_video_input_LDADD= $(top_builddir)/src/libring.la $(top_builddir)/src/media/video/libvideo.la $(YAML_LIBS)

##################
# EXCLUDED_TEST SUITES #
##################
# test_video_rtp
#	reason: compilation fails
#	test_video_rtp.cpp:38:19: error: no matching function for call to ‘ring::video::VideoRtpSession::start()’
#
#	test_video_rtp_SOURCES= media/video/test_video_rtp.cpp
#	test_video_rtp_LDADD= $(top_builddir)/src/libring.la $(top_builddir)/src/media/video/libvideo.la $(YAML_LIBS)
#
#
# test_shm (compilation fails, include file not in repository?)
#	reason: compilation fails, include file not in repository:
#	test_video_input.cpp.cpp:21:22: fatal error: shm_sink.h: No such file or directory
#
# 	if HAVE_LINUX
# 	test_shm_SOURCES= media/video/test_shm.cpp media/video/shm_src.cpp media/video/shm_src.h
# 	test_shm_LDADD= $(top_builddir)/src/libring.la $(top_builddir)/src/media/video/libvideo.la $(YAML_LIBS) -lrt
# 	test_shm_CXXFLAGS=$(AM_CXXFLAGS)
# 	endif

TESTS= $(check_PROGRAMS)
