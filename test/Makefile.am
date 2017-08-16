# Rules for the test code (use `make check` to execute)
include $(top_srcdir)/globals.mk

SUBDIRS=sip
SUBDIRS+=unitTest

AM_CXXFLAGS=-I$(top_srcdir)/src/media/video -I$(top_srcdir)/src
check_PROGRAMS=

####### Unit Test ########
#
# base64
#
check_PROGRAMS+= unit_test_base64
unit_test_base64_SOURCES= unitTest/base64/base64.cpp
unit_test_base64_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

#
# video_input
#
check_PROGRAMS+= unit_test_video_input
unit_test_video_input_SOURCES= unitTest/media/video/video_input/testVideo_input.cpp
unit_test_video_input_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

#
#smartools
#
check_PROGRAMS+= unit_test_smartools
unit_test_smartools_SOURCES= unitTest/smartools/testSmartools.cpp
unit_test_smartools_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

#
# account_factory
#
check_PROGRAMS+= unit_test_account_factory
unit_test_account_factory_SOURCES= unitTest/account_factory/testAccount_factory.cpp
unit_test_account_factory_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la


#
# string_utils
#
check_PROGRAMS+= unit_test_string_utils
unit_test_string_utils_SOURCES= unitTest/string_utils/testString_utils.cpp
unit_test_string_utils_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

#
# fileutils
#
check_PROGRAMS+= unit_test_fileutils
unit_test_fileutils_SOURCES= unitTest/fileutils/testFileutils.cpp
unit_test_fileutils_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

#
# map_utils
#
check_PROGRAMS+= unit_test_map_utils
unit_test_map_utils_SOURCES= unitTest/map_utils/testMap_utils.cpp
unit_test_map_utils_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

#
# utf8_utils
#
check_PROGRAMS+= unit_test_utf8_utils
unit_test_utf8_utils_SOURCES= unitTest/utf8_utils/testUtf8_utils.cpp
unit_test_utf8_utils_LDADD= $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

TESTS= $(check_PROGRAMS)
