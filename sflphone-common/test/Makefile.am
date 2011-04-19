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
	instantmessagingtest.h \
	instantmessagingtest.cpp \
	siptest.h \
	siptest.cpp \
	sdptest.h \
	sdptest.cpp

LLIBS=$(CPPUNIT_LIBS) \
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
	../src/sflphoned-preferences.o \
	../src/libsflphone.la
