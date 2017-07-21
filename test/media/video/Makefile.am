include $(top_srcdir)/globals.mk

check_PROGRAMS=
#
# video_input testsuite
#
check_PROGRAMS+= test_video_input
test_video_input_SOURCES= test_video_input.cpp test_video_input.h
test_video_input_LDADD= $(top_builddir)/src/libring.la

TESTS= $(check_PROGRAMS)
