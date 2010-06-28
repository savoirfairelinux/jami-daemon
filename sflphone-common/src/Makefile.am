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
		managerimpl_registration.cpp \
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

if USE_NETWORKMANAGER
NETWORKMANAGER=-DUSE_NETWORKMANAGER
else
NETWORKMANAGER=
endif

sflphoned_CXXFLAGS = \
		-DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(IAX_CXXFLAG) $(NETWORKMANAGER) \
		-DVERSION=\"$(VERSION)\" \
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
		-DVERSION=\"$(VERSION)\"
# libsflphone 

sflphoned_LDADD = ./libsflphone.la
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
	$(src)/libs/dbus-c++/src/libdbus-c++-1.la \
	$(IAX_LIB) \
	./sip/libsiplink.la \
	./sip/im/libim.la \
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

libsflphone_la_LDFLAGS = \
		@CCGNU2_LIBS@ \
		@CCEXT2_LIBS@ \
		@ZRTPCPP_LIBS@ \
		@GSTREAMER_LIBS@ \
		@LIBAVCODEC_LIBS@ \
		@LIBSWSCALE_LIBS@ \
		$(PJSIP_LIBS) \
		@CCRTP_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@SAMPLERATE_LIBS@ \
		@libssl_LIBS@ \
		@UUID_LIBS@

libsflphone_la_CFLAGS = \
		@CCGNU2_CFLAGS@ \
		@CCEXT2_CFLAGS@ \
		@ZRTPCPP_CFLAGS@ \
		@GSTREAMER_CFLAGS@ \
		@LIBAVCODEC_CFLAGS@ \
		@LIBSWSCALE_CFLAGS@ \
		$(PJSIP_CFLAGS) \
		@CCRTP_CFLAGS@ \
		@ALSA_CFLAGS@ \
		@PULSEAUDIO_CFLAGS@ \
		@SAMPLERATE_CFLAGS@ \
		@libssl_CFLAGS@ \
		@UUID_CFLAGS@

libsflphone_la_SOURCES =

indent:
	@echo "Indenting code:"
	if [ -f $(ASTYLERC) ] ; then \
		$(indent) --options=$(ASTYLERC) --recursive *.cpp *.h; \
	fi

