include ../globals.mak

TESTS_ENVIRONMENT = CODECS_PATH="$(top_builddir)/src/audio/codecs"
check_PROGRAMS = test

TESTS = run_tests.sh

test_CXXFLAGS = -DWORKSPACE="$(top_builddir)/test/"
test_LDADD = $(top_builddir)/src/libsflphone.la @ZRTPCPP_LIBS@ @LIBCRYPTO_LIBS@ @CPPUNIT_LIBS@ @YAML_LIBS@

EXTRA_DIST = sflphoned-sample.yml history-sample.tpl run_tests.sh
test_SOURCES = constants.h \
			   test_utils.h \
			   main.cpp \
			   validator.h \
			   validator.cpp \
			   accounttest.h \
			   accounttest.cpp \
			   audiolayertest.h \
			   audiolayertest.cpp \
			   configurationtest.h \
			   configurationtest.cpp \
			   historytest.h \
			   historytest.cpp \
			   numbercleanertest.h \
			   numbercleanertest.cpp \
			   sdesnegotiatortest.h \
			   sdesnegotiatortest.cpp \
			   instantmessagingtest.h \
			   instantmessagingtest.cpp \
			   siptest.h \
			   siptest.cpp \
			   sdptest.h \
			   sdptest.cpp \
			   echocanceltest.h \
			   echocanceltest.cpp \
			   gaincontroltest.h \
			   gaincontroltest.cpp \
			   mainbuffertest.h \
			   mainbuffertest.cpp
