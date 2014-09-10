include ../globals.mak

TESTS_ENVIRONMENT = CODECS_PATH="$(top_builddir)/src/audio/codecs"
check_PROGRAMS = test

TESTS = run_tests.sh

test_CXXFLAGS = -DWORKSPACE="$(top_srcdir)/test/" @PTHREAD_CFLAGS@
test_LDADD = $(top_builddir)/src/libsflphone.la @ZRTPCPP_LIBS@ @CPPUNIT_LIBS@ @YAML_LIBS@ @CCRTP_LIBS@

if BUILD_INSTANT_MESSAGING
test_LDADD += @EXPAT_LIBS@
endif

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
