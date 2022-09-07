# Rules for the test code (use `make check` to execute)
include $(top_srcdir)/globals.mk

AM_CXXFLAGS += -I$(top_srcdir)/src
AM_LDFLAGS += $(CPPUNIT_LIBS) $(top_builddir)/src/libring.la

check_PROGRAMS = ut_sip
ut_sip_SOURCES = sip.cpp test_SIP.h test_SIP.cpp

TESTS = $(check_PROGRAMS)
