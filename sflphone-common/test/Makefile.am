include ../globals.mak


noinst_PROGRAMS = numbercleanerTester pluginmanagerTester hookmanagerTester audiolayerTester historyTester mainbufferTester sdesnegotiatorTester #rtpTester

OBJECT_FILES= \
	../src/sflphoned-logger.o \
	../src/sflphoned-managerimpl.o \
	../src/sflphoned-account.o\
	../src/sflphoned-accountcreator.o \
	../src/sflphoned-call.o \
	../src/sflphoned-conference.o \
	../src/sflphoned-eventthread.o \
	../src/sflphoned-managerimpl_registration.o \
	../src/sflphoned-numbercleaner.o \
	../src/sflphoned-observer.o \
	../src/sflphoned-voiplink.o \
	../src/sip/libsiplink_la-sipcall.o \
	../src/iax/libiaxlink_la-iaxcall.o \
	../src/sip/libsiplink_la-sipvoiplink.o \
	../src/iax/libiaxlink_la-iaxvoiplink.o \
	../src/sip/libsiplink_la-sipaccount.o \
	../src/iax/libiaxlink_la-iaxaccount.o \
	../src/sip/libsiplink_la-sdp.o \
	../src/sip/libsiplink_la-sdpmedia.o \
	../src/plug-in/pluginmanager.o \
	../src/plug-in/audiorecorder/audiorecord.o \
	../src/audio/samplerateconverter.o \
        ../src/history/historymanager.o
	../

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
		@ZRTPCPP_LIBS@ \
		@libssl_LIBS@ \
		$(PJSIP_LIBS) \
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
		@ZRTPCPP_LIBS@ \
		@libssl_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
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
		@ZRTPCPP_LIBS@ \
		@libssl_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
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
		@ZRTPCPP_LIBS@ \
		@libssl_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		$(OBJECT_FILES)

historyTester_SOURCES = \
		historyTest.h \
		historyTest.cpp \
		TestMain.cpp

historyTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@ZRTPCPP_LIBS@ \
		@libssl_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		$(OBJECT_FILES)


mainbufferTester_SOURCES = \
		mainbufferTest.h \
		mainbufferTest.cpp \
		TestMain.cpp

mainbufferTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@ZRTPCPP_LIBS@ \
		@libssl_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		$(OBJECT_FILES)

sdesnegotiatorTester_SOURCES = \
		sdesnegotiatorTest.h \
		sdesnegotiatorTest.cpp \
		TestMain.cpp

sdesnegotiatorTester_LDADD = \
		../src/libsflphone.la  \
		$(SFLPHONE_LIBS) $(ZEROCONFLIB) $(LIB_DNSSD) \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@CPPUNIT_LIBS@ \
		@CCEXT2_LIBS@ \
		@CCGNU2_LIBS@ \
		@CCRTP_LIBS@ \
		@ZRTPCPP_LIBS@ \
		@libssl_LIBS@ \
		@SAMPLERATE_LIBS@ \
		$(PJSIP_LIBS) \
		$(OBJECT_FILES)
