# Rules for the test code (use `make check` to execute)
include ../../globals.mk
TESTS = sip
check_PROGRAMS = $(TESTS)
sip_LDADD = ./../../src/libring.la
sip_SOURCES = sip.cpp test_SIP.h test_SIP.cpp
sip_CXXFLAGS = $(CPPUNIT_CFLAGS)
sip_LDFLAGS = $(CPPUNIT_LIBS) -ldl
