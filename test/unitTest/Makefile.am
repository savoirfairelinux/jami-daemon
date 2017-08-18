# Rules for the test code (use `make check` to execute)
include $(top_srcdir)/globals.mk

AM_CXXFLAGS = -I$(top_srcdir)/src
AM_LDFLAGS = $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la
check_PROGRAMS =

####### Unit Test ########

#
# account_factory
#
check_PROGRAMS += ut_account_factory
ut_account_factory_SOURCES = account_factory/testAccount_factory.cpp

#
# base64
#
check_PROGRAMS += ut_base64
ut_base64_SOURCES = base64/base64.cpp

#
# channel
#
check_PROGRAMS += ut_channel
ut_channel_SOURCES = channel/testChannel.cpp

#
# map_utils
#
check_PROGRAMS += ut_map_utils
ut_map_utils_SOURCES = map_utils/testMap_utils.cpp

#
# fileutils
#
check_PROGRAMS += ut_fileutils
ut_fileutils_SOURCES = fileutils/testFileutils.cpp

#
# smartools
#
check_PROGRAMS += ut_smartools
ut_smartools_SOURCES = smartools/testSmartools.cpp

#
# utf8_utils
#
check_PROGRAMS += ut_utf8_utils
ut_utf8_utils_SOURCES = utf8_utils/testUtf8_utils.cpp

#
# string_utils
#
check_PROGRAMS += ut_string_utils
ut_string_utils_SOURCES = string_utils/testString_utils.cpp

#
# video_input
#
check_PROGRAMS += ut_video_input
ut_video_input_SOURCES = media/video/testVideo_input.cpp

TESTS = $(check_PROGRAMS)
