# Rules for the test code (use `make check` to execute)
include $(top_srcdir)/globals.mk

AM_CXXFLAGS = -I$(top_srcdir)/src
AM_LDFLAGS = $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

check_PROGRAMS = test_turn
test_turn_SOURCES = main.cpp test_turn.h test_TURN.cpp

TESTS = $(check_PROGRAMS)
