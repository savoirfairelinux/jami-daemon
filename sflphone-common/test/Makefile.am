include ../globals.mak

bin_PROGRAMS = numbercleanerTester pluginmanagerTester hookmanagerTester audiolayerTester rtpTester

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
	../src/plug-in/pluginmanager.o \
	../src/plug-in/audiorecorder/audiorecord.o \
	../src/sflphoned-samplerateconverter.o \
	../src/sflphoned-sdp.o \
	../src/sflphoned-sdpmedia.o \
	../src/sflphoned-numbercleaner.o

numbercleanerTester_SOURCES = \
		numbercleanerTest.h \
		numbercleanerTest.cpp \
		TestMain.cpp

numbercleanerTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		-luuid \
		$(OBJECT_FILES)

pluginmanagerTester_SOURCES = \
		pluginmanagerTest.h \
		pluginmanagerTest.cpp \
		TestMain.cpp

pluginmanagerTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		-luuid \
		$(OBJECT_FILES)
						
hookmanagerTester_SOURCES = \
		hookmanagerTest.cpp \
		TestMain.cpp

hookmanagerTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		-luuid \
		$(OBJECT_FILES)

audiolayerTester_SOURCES = \
		audiolayerTest.h \
		audiolayerTest.cpp \
		TestMain.cpp

audiolayerTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		-luuid \
		$(OBJECT_FILES)

rtpTester_SOURCES = \
		rtpTest.h \
		rtpTest.cpp \
		TestMain.cpp

rtpTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		-luuid \
		$(OBJECT_FILES)

