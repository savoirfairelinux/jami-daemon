# Rules for the test code (use `make check` to execute)
include $(top_srcdir)/globals.mk

SUBDIRS=sip
SUBDIRS+=base64
SUBDIRS+=media
SUBDIRS+=unitTest

AM_CXXFLAGS=-I$(top_srcdir)/src/media/video -I$(top_srcdir)/src
check_PROGRAMS=

#
# base64
#
check_PROGRAMS+= test_base64
test_base64_SOURCES= base64/base64.cpp
test_base64_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

#
# video_input
#
check_PROGRAMS+= test_video_input
test_video_input_SOURCES= media/video/test_video_input.cpp media/video/test_video_input.h
test_video_input_LDADD= $(top_builddir)/src/libring.la $(top_builddir)/src/media/video/libvideo.la $(YAML_LIBS)

#
# sip
#
# error calls.cbegin() fail
#check_PROGRAMS += test_sip
#test_sip_LDADD = $(top_builddir)/src/libring.la
#test_sip_SOURCES = sip/sip.cpp sip/test_SIP.h sip/test_SIP.cpp sip/../../manager.h
#test_sip_CXXFLAGS = $(CPPUNIT_CFLAGS)
#test_sip_LDFLAGS = $(CPPUNIT_LIBS) -ldl

####### Unit Test ########

#
# smartools
#
check_PROGRAMS+= unit_test_smartools
unit_test_smartools_SOURCES= unitTest/smartools/testSmartools.cpp
unit_test_smartools_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

TESTS= $(check_PROGRAMS)
