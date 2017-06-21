# Rules for the test code (use `make check` to execute)
include $(top_srcdir)/globals.mk
TESTS = sip
check_PROGRAMS = $(TESTS)
sip_LDADD = $(top_builddir)/src/libring.la
sip_SOURCES = sip.cpp test_SIP.h test_SIP.cpp ../../manager.h
sip_CXXFLAGS = $(CPPUNIT_CFLAGS)
sip_LDFLAGS = $(CPPUNIT_LIBS) -ldl
