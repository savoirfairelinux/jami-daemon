include ../globals.mak

libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = sflphoned

if SFL_VIDEO
SFL_VIDEO_SUBDIR = video
SFL_VIDEO_LIB=./video/libvideo.la
endif

if BUILD_INSTANT_MESSAGING
INSTANT_MESSAGING_SUBDIR = im
IM_LIB=./im/libim.la
endif

# Redefine the USE_IAX variable here, so that it could be used in managerimpl
if USE_IAX
IAX_SUBDIR=iax
IAX_CXXFLAG=-DUSE_IAX
IAX_LIB=./iax/libiaxlink.la $(top_builddir)/libs/iax2/libiax2.la
endif

if USE_NETWORKMANAGER
NETWORKMANAGER=-DUSE_NETWORKMANAGER
endif

SUBDIRS = client audio config hooks history sip $(IAX_SUBDIR) $(INSTANT_MESSAGING_SUBDIR) $(SFL_VIDEO_SUBDIR)

sflphoned_SOURCES = main.cpp

sflphoned_CXXFLAGS = \
		-DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(IAX_CXXFLAG) $(NETWORKMANAGER) \
		-DVERSION=\"$(VERSION)\"

# libsflphone

sflphoned_LDADD = ./libsflphone.la $(libssl_LIBS) -lcrypto $(YAML_LIBS)
noinst_LTLIBRARIES = libsflphone.la

libsflphone_la_LIBADD = \
	$(IAX_LIB) \
	./sip/libsiplink.la \
	./audio/libaudio.la \
	./client/libclient.la \
	./config/libconfig.la \
	./hooks/libhooks.la \
	./history/libhistory.la $(SFL_VIDEO_LIB) $(IM_LIB)

libsflphone_la_LDFLAGS = \
		@CCGNU2_LIBS@ \
		@CCEXT2_LIBS@ \
		@ZRTPCPP_LIBS@ \
		$(PJSIP_LIBS) \
		@CCRTP_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@SAMPLERATE_LIBS@ \
		@SNDFILE_LIBS@ \
		@libssl_LIBS@ \
		@UUID_LIBS@ \
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
		@DBUSCPP_CFLAGS@

libsflphone_la_SOURCES = conference.cpp \
		voiplink.cpp \
		preferences.cpp \
		managerimpl.cpp \
		manager.cpp \
		eventthread.cpp \
		call.cpp \
		account.cpp \
		logger.cpp \
		numbercleaner.cpp \
		fileutils.cpp \
		sflthread.cpp \
		ip_utils.cpp \
		sflthread.h \
		conference.h \
		voiplink.h \
		preferences.h \
		managerimpl.h \
		manager.h \
		global.h \
		eventthread.h \
		account.h \
		call.h \
		logger.h \
		numbercleaner.h \
		fileutils.h \
		noncopyable.h \
		sfl_types.h \
		array_size.h \
		account_schema.h \
		registration_states.h \
		map_utils.h
