include $(top_srcdir)/globals.mk
include ../globals.mak

AM_CXXFLAGS=-I$(top_srcdir)/src/media/video -I$(top_srcdir)/src

check_PROGRAMS= base64 test_video_input
TESTS= $(check_PROGRAMS)

##################
# EXCLUDED_TESTS #
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

base64_SOURCES= base64.cpp
base64_LDADD= $(BOOST_FILESYSTEM_LIB) $(BOOST_SYSTEM_LIB) $(BOOST_UNIT_TEST_FRAMEWORK_LIB) $(top_builddir)/src/libring.la

test_video_input_SOURCES= media/video/test_video_input.cpp media/video/test_video_input.h
test_video_input_LDADD= $(top_builddir)/src/libring.la $(top_builddir)/src/media/video/libvideo.la $(YAML_LIBS)

TESTS_ENVIRONMENT = CODECS_PATH="$(top_builddir)/src/audio/codecs"
check_PROGRAMS = test

TESTS = run_tests.sh

test_CXXFLAGS = -DWORKSPACE="$(top_srcdir)/test/" @PTHREAD_CFLAGS@
test_LDADD = $(top_builddir)/src/libsflphone.la @ZRTPCPP_LIBS@ @LIBCRYPTO_LIBS@ @CPPUNIT_LIBS@ @YAML_LIBS@

EXTRA_DIST = sflphoned-sample.yml history-sample.tpl run_tests.sh
test_SOURCES = constants.h \
			   test_utils.h \
			   main.cpp \
			   accounttest.h \
			   accounttest.cpp \
			   audiocodectest.h \
			   audiocodectest.cpp \
			   audiolayertest.h \
			   audiolayertest.cpp \
			   configurationtest.h \
			   configurationtest.cpp \
			   historytest.h \
			   historytest.cpp \
			   numbercleanertest.h \
			   numbercleanertest.cpp \
			   siptest.h \
			   siptest.cpp \
			   sdptest.h \
			   sdptest.cpp \
			   mainbuffertest.h \
			   mainbuffertest.cpp \
			   resamplertest.h \
			   resamplertest.cpp \
			   hooktest.h \
			   hooktest.cpp \
			   audiobuffertest.h \
			   audiobuffertest.cpp \
			   iptest.h \
			   iptest.cpp

if BUILD_SDES
test_SOURCES+=sdesnegotiatortest.h \
		sdesnegotiatortest.cpp
endif

if BUILD_INSTANT_MESSAGING
test_SOURCES+=instantmessagingtest.h \
			   instantmessagingtest.cpp
endif

if BUILD_TLS
test_SOURCES+=tlstest.h \
				tlstest.cpp
endif

clean-local:
	rm -rf cppunitresults.xml im:testfile1.txt im:testfile2.txt \
		sample_echocancel_500ms_8kHz_16bit.raw
