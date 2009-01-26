include ../globals.mak

bin_PROGRAMS = configurationTester pluginmanagerTester audiorecorderTester

OBJECT_FILES= \
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
	../src/sflphoned-useragent.o \
	../src/plug-in/pluginmanager.o \
	../src/plug-in/audiorecorder/audiorecord.o \
	../src/sflphoned-samplerateconverter.o

configurationTester_SOURCES = \
		configurationTest.cpp \
		configurationTest.h \
		TestMain.cpp

pluginmanagerTester_SOURCES = \
		pluginmanagerTest.h \
		pluginmanagerTest.cpp \
		TestMain.cpp

audiorecorderTester_SOURCES = \
		audiorecorderTest.h \
		audiorecorderTest.cpp \
		TestMain.cpp


configurationTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) $(IAX_LIBS) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@DBUSCPP_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		-luuid \
		$(OBJECT_FILES)
	
pluginmanagerTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) $(IAX_LIBS) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@DBUSCPP_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		-luuid \
		$(OBJECT_FILES)
	
audiorecorderTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) $(IAX_LIBS) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@DBUSCPP_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		-luuid \
		$(OBJECT_FILES)
							
