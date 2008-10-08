include ../globals.mak

bin_PROGRAMS = configurationTester

#ALSAFLAG= -lasound
PULSEAUDIO_LIBS=-lpulse

configurationTester_SOURCES =  configurationTest.cpp  \
				TestMain.cpp

configurationTester_LDADD =  ../src/libsflphone.la  \
				$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) $(IAX_LIBS) $(EXOSIP_LIBS) @ALSA_LIBS@ $(PULSEAUDIO_LIBS) @CPPUNIT_LIBS@ \
				../src/sflphoned-managerimpl.o \
				../src/sflphoned-accountcreator.o \
				../src/sflphoned-call.o \
				../src/sflphoned-sipcall.o \
				../src/sflphoned-iaxcall.o \
				../src/sflphoned-voiplink.o \
				../src/sflphoned-sipvoiplink.o \
				../src/sflphoned-iaxvoiplink.o \
				../src/sflphoned-account.o \
				../src/sflphoned-sipaccount.o \
				../src/sflphoned-iaxaccount.o \
				../src/sflphoned-eventthread.o \
				../src/sflphoned-samplerateconverter.o
