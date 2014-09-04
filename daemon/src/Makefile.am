include ../globals.mak

if SFL_VIDEO
SFL_VIDEO_SUBDIR = video
SFL_VIDEO_LIBS=./video/libvideo.la \
			   ./video/v4l2/libv4l2.la
endif

if BUILD_INSTANT_MESSAGING
INSTANT_MESSAGING_SUBDIR = im
IM_LIBA=./im/libim.la
IM_LIB=@EXPAT_LIBS@
endif

# Redefine the USE_IAX variable here, so that it could be used in managerimpl
if USE_IAX
IAX_SUBDIR=iax
IAX_CXXFLAG=-DUSE_IAX
IAX_LIBA=./iax/libiaxlink.la
IAX_LIB=-liax
endif

if USE_NETWORKMANAGER
NETWORKMANAGER=-DUSE_NETWORKMANAGER
endif

if BUILD_TLS
TLS_LIB = @GNUTLS_LIBS@
TLS_CFLAGS = @GNUTLS_CFLAGS@
endif

SUBDIRS = client audio config hooks history sip $(IAX_SUBDIR) $(INSTANT_MESSAGING_SUBDIR) $(SFL_VIDEO_SUBDIR)

if USE_DHT
SUBDIRS += dht
endif

# libsflphone

include_HEADERS = sflphone.h

lib_LTLIBRARIES = libsflphone.la

libsflphone_la_LIBADD = \
	./sip/libsiplink.la \
	./audio/libaudio.la \
	./client/libclient.la \
	./config/libconfig.la \
	./hooks/libhooks.la \
	./history/libhistory.la \
	$(IAX_LIBA) \
	$(IM_LIBA) \
	$(SFL_VIDEO_LIBS)

libsflphone_la_LDFLAGS = \
		@ZRTPCPP_LIBS@ \
		@PJPROJECT_LIBS@ \
		@CCRTP_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@SAMPLERATE_LIBS@ \
		@SNDFILE_LIBS@ \
		@YAMLCPP_LIBS@ \
		$(TLS_LIB) \
		$(IAX_LIB) \
		$(IM_LIB) \
		$(PCRE_LIBS)

libsflphone_la_CFLAGS = \
		@ZRTPCPP_CFLAGS@ \
		@PJPROJECT_CFLAGS@ \
		@CCRTP_CFLAGS@ \
		@ALSA_CFLAGS@ \
		@PULSEAUDIO_CFLAGS@ \
		@SAMPLERATE_CFLAGS@ \
		$(TLS_CFLAGS)

libsflphone_la_SOURCES = conference.cpp \
		account_factory.cpp \
		call_factory.cpp \
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
		account factory.h \
		call factory.h \
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
		map_utils.h \
		sflphone.h \
		sflphone_api.cpp
