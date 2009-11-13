include ../globals.mak

libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = sflphoned

SUBDIRS = dbus audio config plug-in hooks history sip iax

# Add here the cpp files to be build with sflphone
sflphoned_SOURCES = \
		conference.cpp \
		voiplink.cpp \
		main.cpp \
		managerimpl.cpp \
		observer.cpp \
		eventthread.cpp \
		accountcreator.cpp \
		call.cpp \
		account.cpp \
		logger.cpp \
		numbercleaner.cpp 

# Redefine the USE_IAX variable here, so that it could be used in managerimpl
if USE_IAX
IAX_CXXFLAG=-DUSE_IAX
IAX_LIB=./iax/libiaxlink.la
else
IAX_CXXFLAG=
IAX_LIB=
endif

sflphoned_CXXFLAGS = \
		-DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(IAX_CXXFLAG)\
		@ZRTPCPP_CFLAGS@ \
		@libssl_CFLAGS@


# Add here the dynamic libraries sflphoned should be linked against
sflphoned_LDADD = \
		./libsflphone.la \
		@CCGNU2_LIBS@ \
		@CCEXT2_LIBS@ \
		@ZRTPCPP_LIBS@ \
		$(PJSIP_LIBS) \
		@CCRTP_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@SAMPLERATE_LIBS@ \
		@libssl_LIBS@

# sflphoned_LDFLAGS= -pg -luuid
sflphoned_LDFLAGS= $(UUID_LIBS)

noinst_LTLIBRARIES = libsflphone.la

noinst_HEADERS = \
		conference.h \
        	voiplink.h \
		managerimpl.h \
		manager.h \
		global.h \
		observer.h \
		eventthread.h \
		user_cfg.h \
		account.h \
		accountcreator.h \
		call.h \
		logger.h \
		numbercleaner.h

libsflphone_la_LIBADD = \
	$(src)/libs/utilspp/libutilspp.la \
	$(src)/libs/iax2/libiax2.la \
	$(IAX_LIB) \
	./sip/libsiplink.la \
	./audio/libaudio.la \
	./audio/audiortp/libaudiortp.la \
	./audio/sound/libsound.la \
	./audio/codecs/libcodecdescriptor.la \
	./audio/alsa/libalsalayer.la \
	./audio/pulseaudio/libpulselayer.la \
	./dbus/libdbus.la \
	./config/libconfig.la \
	./plug-in/libplugin.la \
	./plug-in/audiorecorder/libaudiorecorder.la \
	./hooks/libhooks.la \
	./history/libhistory.la

libsflphone_la_SOURCES =
