include ../globals.mak

libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = sflphoned

# all: indent

SUBDIRS = dbus audio config plug-in hooks history sip iax im

sflphoned_SOURCES = main.cpp

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
		-DVERSION=\"$(VERSION)\"

# libsflphone 

sflphoned_LDADD = ./libsflphone.la $(libssl_LIBS) -lcrypto
noinst_LTLIBRARIES = libsflphone.la

noinst_HEADERS = \
		conference.h \
		voiplink.h \
		preferences.h \
		managerimpl.h \
		manager.h \
		global.h \
		eventthread.h \
		account.h \
		accountcreator.h \
		call.h \
		logger.h \
		numbercleaner.h \
		fileutils.h

libsflphone_la_LIBADD = \
	$(src)/libs/utilspp/libutilspp.la \
	$(src)/libs/iax2/libiax2.la \
	$(IAX_LIB) \
	./im/libim.la \
	./sip/libsiplink.la \
	./audio/libaudio.la \
	./dbus/libdbus.la \
	./config/libconfig.la \
	./plug-in/libplugin.la \
	./hooks/libhooks.la \
	./history/libhistory.la

libsflphone_la_LDFLAGS = \
		@CCGNU2_LIBS@ \
		@CCEXT2_LIBS@ \
		@ZRTPCPP_LIBS@ \
		$(PJSIP_LIBS) \
		@CCRTP_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@SAMPLERATE_LIBS@ \
		@libssl_LIBS@ \
		@UUID_LIBS@ \
		@yaml_LIBS@ \
		@xml_LIBS@ \
		@DBUSCPP_LIBS@

libsflphone_la_CFLAGS = \
		@CCGNU2_CFLAGS@ \
		@CCEXT2_CFLAGS@ \
		@ZRTPCPP_CFLAGS@ \
		$(PJSIP_CFLAGS) \
		@CCRTP_CFLAGS@ \
		@ALSA_CFLAGS@ \
		@PULSEAUDIO_CFLAGS@ \
		@SAMPLERATE_CFLAGS@ \
		@libssl_CFLAGS@ \
		@UUID_CFLAGS@ \
		@yaml_CFLAGS@ \
	 	@xml_CFLAGS@

libsflphone_la_SOURCES = conference.cpp \
		voiplink.cpp \
		preferences.cpp \
		managerimpl.cpp \
		managerimpl_registration.cpp \
		eventthread.cpp \
		accountcreator.cpp \
		call.cpp \
		account.cpp \
		logger.cpp \
		numbercleaner.cpp \
		fileutils.cpp
