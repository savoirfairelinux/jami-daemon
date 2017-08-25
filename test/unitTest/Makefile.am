# Rules for the test code (use `make check` to execute)
include $(top_srcdir)/globals.mk

AM_CXXFLAGS = -I$(top_srcdir)/src
AM_LDFLAGS = $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la
check_PROGRAMS =

####### Unit Test ########

#
# base64
#
check_PROGRAMS += ut_base64
ut_base64_SOURCES = base64/base64.cpp

#
# smartools
#
check_PROGRAMS += ut_smartools
ut_smartools_SOURCES = smartools/testSmartools.cpp

#
# video_input
#
check_PROGRAMS += ut_video_input
ut_video_input_SOURCES = media/video/testVideo_input.cpp

TESTS = $(check_PROGRAMS)
