include ../globals.mak

libexecdir=$(libdir)/sflphone
libexec_PROGRAMS = sflphoned

if SFL_VIDEO
SFL_VIDEO_SUBDIR = video
SFL_VIDEO_LIBS=./video/libvideo.la \
			   ./video/v4l2/libv4l2.la
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

if BUILD_TLS
TLS_LIB = @GNUTLS_LIBS@
TLS_CFLAGS = @GNUTLS_CFLAGS@
endif

SUBDIRS = client audio config hooks history sip $(IAX_SUBDIR) $(INSTANT_MESSAGING_SUBDIR) $(SFL_VIDEO_SUBDIR)

sflphoned_SOURCES = main.cpp

sflphoned_CXXFLAGS = \
		-DPREFIX=\"$(prefix)\" -DPROGSHAREDIR=\"${datadir}/sflphone\" $(IAX_CXXFLAG) $(NETWORKMANAGER) \
		-DVERSION=\"$(VERSION)\"

# libsflphone

sflphoned_LDADD = ./libsflphone.la $(YAML_LIBS)
noinst_LTLIBRARIES = libsflphone.la

libsflphone_la_LIBADD = \
	$(IAX_LIB) \
	./sip/libsiplink.la \
	./audio/libaudio.la \
	./client/libclient.la \
	./config/libconfig.la \
	./hooks/libhooks.la \
	./history/libhistory.la $(SFL_VIDEO_LIBS) $(IM_LIB)

libsflphone_la_LDFLAGS = \
		@ZRTPCPP_LIBS@ \
		$(PJSIP_LIBS) \
		@CCRTP_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@SAMPLERATE_LIBS@ \
		@SNDFILE_LIBS@ \
		$(TLS_LIB) \
		@DBUSCPP_LIBS@ \
		@YAML_LIBS@ \
		$(IAX_LIB) \
		$(IM_LIB) \
		$(PCRE_LIBS)

if BUILD_INSTANT_MESSAGING
libsflphone_la_LDFLAGS += @EXPAT_LIBS@
endif

if BUILD_SDES
libsflphone_la_LDFLAGS += @PCRE_LIBS@
endif

libsflphone_la_CFLAGS = \
		@ZRTPCPP_CFLAGS@ \
		$(PJSIP_CFLAGS) \
		@CCRTP_CFLAGS@ \
		@ALSA_CFLAGS@ \
		@PULSEAUDIO_CFLAGS@ \
		@SAMPLERATE_CFLAGS@ \
		$(TLS_CFLAGS) \
		@DBUSCPP_CFLAGS@

libsflphone_la_SOURCES = conference.cpp \
		voiplink.cpp \
		preferences.cpp \
		managerimpl.cpp \
		manager.cpp \
		call.cpp \
		account.cpp \
		logger.c \
		numbercleaner.cpp \
		fileutils.cpp \
		threadloop.cpp \
		ip_utils.cpp \
		utf8_utils.cpp \
		threadloop.h \
		conference.h \
		voiplink.h \
		preferences.h \
		managerimpl.h \
		manager.h \
		global.h \
		account.h \
		call.h \
		logger.h \
		numbercleaner.h \
		fileutils.h \
		noncopyable.h \
		utf8_utils.h \
		sfl_types.h \
		array_size.h \
		account_schema.h \
		registration_states.h \
		map_utils.h
