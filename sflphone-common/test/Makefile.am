include ../globals.mak

check_PROGRAMS = test

test_CXXFLAGS = $(CPPUNIT_CFLAGS)
test_LDADD = $(LLIBS)

test_SOURCES = \
	main.cpp \
	validator.cpp \
	validator.h \
	accounttest.h \
	accounttest.cpp \
	audiolayertest.h \
	audiolayertest.cpp \
	configurationtest.h \
	configurationtest.cpp \
	historytest.h \
	historytest.cpp \
	hookmanagertest.h \
	hookmanagertest.cpp \
	numbercleanertest.h \
	numbercleanertest.cpp \
	pluginmanagertest.h \
	pluginmanagertest.cpp \
	rtptest.h \
	rtptest.cpp \
	sdesnegotiatortest.h \
	sdesnegotiatortest.cpp \
	delaydetectiontest.h \
	delaydetectiontest.cpp \
	instantmessagingtest.cpp




LLIBS=$(CPPUNIT_LIBS) \
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
	../src/libsflphone.la
