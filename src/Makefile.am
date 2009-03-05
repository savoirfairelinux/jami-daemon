include ../globals.mak

bin_PROGRAMS = sflphoned

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

SUBDIRS = audio config dbus plug-in

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
		sdp.cpp \
		sdpmedia.cpp \
		$(IAXSOURCES) 

sflphoned_CXXFLAGS = \
		-DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" \
		$(IAX_FLAGS)


# Add here the dynamic libraries sflphoned should be linked against
sflphoned_LDADD = \
		./libsflphone.la \
		@CCGNU2_LIBS@ \
		@CCEXT2_LIBS@ \
		$(IAX_LIBS) \
		$(PJSIP_LIBS) \
		@DBUSCPP_LIBS@ \
		@CCRTP_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@SAMPLERATE_LIBS@ 

#sflphoned_LDFLAGS=-pg

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
		sdp.h \
		sdpmedia.h
		
libsflphone_la_LIBADD = \
	$(src)/libs/stund/libstun.la \
    	$(src)/libs/utilspp/libutilspp.la \
	./audio/libaudio.la \
	./dbus/libdbus.la \
	./config/libconfig.la \
	./plug-in/libplugin.la \
	./plug-in/audiorecorder/libaudiorecorder.la \
	$(IAX_LIBS) 

libsflphone_la_SOURCES =
