# Rules for the test code (use `make check` to execute)
TESTS = Sip Sipp
check_PROGRAMS = $(TESTS)
Sip_SOURCES = Sip.cpp ComplexNumberTest.h ComplexNumberTest.cpp
Sip_CXXFLAGS = $(CPPUNIT_CFLAGS)
Sip_LDFLAGS = $(CPPUNIT_LIBS) -ldl

Sipp_SOURCES = Sipp.cpp ComplexNumberTest2.h ComplexNumberTest2.cpp
Sipp_CXXFLAGS = $(CPPUNIT_CFLAGS)
Sipp_LDFLAGS = $(CPPUNIT_LIBS) -ldl
