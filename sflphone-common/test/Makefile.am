include ../globals.mak

TESTS = main
check_PROGRAMS = $(TESTS)
main_CXXFLAGS = $(CPPUNIT_CFLAGS)
main_LDADD = $(LLIBS)
main_LDFLAGS = -ldl
main_SOURCES = main.cpp \
	validator.cpp \
	validator.h \
	accounttest.h \
	accounttest.cpp





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
    ../src/history/historymanager.o