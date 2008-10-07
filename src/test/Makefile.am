bin_PROGRAMS = configurationTester

CPPUNIT_LDFLAG = `pkg-config --libs cppunit`

configurationTester_SOURCES = configurationTest.cpp
configurationTester_LDADD = ../.libs/libsflphone.a ../config/.libs/libconfig.a
configurationTester_LDFLAGS = $(CPPUNIT_LDFLAG) 
configurationTester_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs $(libccext2_CFLAGS) $(libccgnu2_CFLAGS) $(IAX_CFLAGS) $(USER_INCLUDES) $(libdbuscpp_CFLAGS)

