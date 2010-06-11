include ../globals.mak

TESTS = test
check_PROGRAMS = $(TESTS)
test_CXXFLAGS = $(CPPUNIT_CFLAGS)
test_LDADD = $(LLIBS)
test_LDFLAGS = -ldl
test_SOURCES = main.cpp \
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
	mainbuffertest.h \
	mainbuffertest.cpp \
	numbercleanertest.h \
	numbercleanertest.cpp \
	pluginmanagertest.h \
	pluginmanagertest.cpp \
	rtptest.h \
	rtptest.cpp \
	sdesnegotiatortest.h \
	sdesnegotiatortest.cpp \
	delaydetectiontest.h \
	delaydetectiontest.cpp




LLIBS=$(CPPUNIT_LIBS) \
	../src/libsflphone.la  \
	$(ZEROCONFLIB) \
	$(LIB_DNSSD) \
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
	../src/audio/delaydetection.o \
        ../src/history/historymanager.o