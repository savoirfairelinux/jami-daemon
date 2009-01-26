include ../globals.mak

bin_PROGRAMS = sflphoned

if USE_ZEROCONF
ZEROCONFDIR = zeroconf
ZEROCONFLIB = zeroconf/libzeroconf.la
ZEROCONFFLAGS = -DUSE_ZEROCONF
else
ZEROCONFDIR =
ZEROCONFLIB =
ZEROCONFFLAGS =  
endif

if USE_IAX
# Use the global instead of locally built
IAX_FLAGS = -DUSE_IAX
IAXSOURCES = iaxaccount.cpp iaxvoiplink.cpp iaxcall.cpp
IAXHEADERS = iaxaccount.h iaxvoiplink.h iaxcall.h
else
IAX_FLAGS =
IAXSOURCES =
IAXHEADERS =
endif

SUBDIRS = audio config dbus $(ZEROCONFDIR)

# Add here the cpp files to be build with sflphone
sflphoned_SOURCES = \
		voiplink.cpp \
		main.cpp \
		managerimpl.cpp \
		observer.cpp \
		samplerateconverter.cpp \
		eventthread.cpp \
		sipaccount.cpp \
		accountcreator.cpp \
        sipvoiplink.cpp \
		call.cpp \
		account.cpp \
		sipcall.cpp \
		$(IAXSOURCES) \
		useragent.cpp

sflphoned_CXXFLAGS = \
		-DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" \
		$(ZEROCONFFLAGS) \
		$(IAX_FLAGS)


# Add here the dynamic libraries sflphoned should be linked against
sflphoned_LDADD = \
		./libsflphone.la \
		@CCGNU2_LIBS@ \
		@CCEXT2_LIBS@ \
		$(ZEROCONFLIB) \
		$(IAX_LIBS) \
		$(PJSIP_LIBS) \
		@DBUSCPP_LIBS@ \
		@CCRTP_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		-luuid \
		@SAMPLERATE_LIBS@ 

noinst_LTLIBRARIES = libsflphone.la

noinst_HEADERS = \
        voiplink.h \
		managerimpl.h \
		manager.h \
		global.h \
		observer.h \
		eventthread.h \
		user_cfg.h \
		samplerateconverter.h \
		account.h \
		sipaccount.h \
		accountcreator.h \
        sipvoiplink.h \
		call.h \
		sipcall.h \
		useragent.h
		
libsflphone_la_LIBADD = \
	$(src)/libs/stund/libstun.la \
    	$(src)/libs/utilspp/libutilspp.la \
	./audio/libaudio.la \
	./dbus/libdbus.la \
	./config/libconfig.la \
	$(IAX_LIBS) 

libsflphone_la_SOURCES =
