include ../globals.mak

RING_VIDEO_LIBS=

if RING_VIDEO
RING_VIDEO_LIBS+=./media/video/libvideo.la
if HAVE_LINUX
RING_VIDEO_LIBS+= \
                ./media/video/v4l2/libv4l2.la
endif
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

if USE_DHT
RINGACC_SUBDIR=ringdht
RINGACC_CXXFLAG=-DUSE_DHT
RINGACC_LIBA=./ringdht/libringacc.la
endif

if BUILD_TLS
TLS_LIB = @GNUTLS_LIBS@
TLS_CFLAGS = @GNUTLS_CFLAGS@
endif

SUBDIRS = client media config hooks history sip upnp $(IAX_SUBDIR) $(RINGACC_SUBDIR) $(INSTANT_MESSAGING_SUBDIR) $(RING_VIDEO_SUBDIR)

# libring

lib_LTLIBRARIES = libring.la

libring_la_LIBADD = \
	./sip/libsiplink.la \
	./media/libmedia.la \
	./client/libclient.la \
	./config/libconfig.la \
	./hooks/libhooks.la \
	./history/libhistory.la \
	./upnp/libupnpcontrol.la \
	$(RINGACC_LIBA) \
	$(IAX_LIBA) \
	$(IM_LIBA) \
	$(RING_VIDEO_LIBS)

libring_la_LDFLAGS = \
		@PJPROJECT_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@SAMPLERATE_LIBS@ \
		@SNDFILE_LIBS@ \
		@YAMLCPP_LIBS@ \
		@SPEEXDSP_LIBS@ \
		@LIBUPNP_LIBS@ \
		$(TLS_LIB) \
		$(IAX_LIB) \
		$(IM_LIB) \
		$(PCRE_LIBS)

if USE_DHT
libring_la_LDFLAGS += $(OPENDHT_LIBS)
endif

if HAVE_OSX
#FIXME necessary for -lintl
libring_la_LDFLAGS += -L/usr/local/opt/gettext/lib
endif

libring_la_CFLAGS = \
		@PJPROJECT_CFLAGS@ \
		@ALSA_CFLAGS@ \
		@PULSEAUDIO_CFLAGS@ \
		@SAMPLERATE_CFLAGS@ \
		@SPEEXDSP_CFLAGS@ \
		@LIBUPNP_CFLAGS@ \
		$(TLS_CFLAGS)

if USE_DHT
libring_la_CFLAGS += $(OPENDHT_CFLAGS)
endif

libring_la_SOURCES = conference.cpp \
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
		ip_utils.h \
		ip_utils.cpp \
		utf8_utils.cpp \
		ice_transport.cpp \
		ice_transport.h \
		plugin_manager.cpp \
		plugin_loader_dl.cpp \
		ring_plugin.h \
		plugin_loader.h \
		plugin_manager.h \
		threadloop.h \
		conference.h \
		account_factory.h \
		call_factory.h \
		preferences.h \
		managerimpl.h \
		manager.h \
		account.h \
		call.h \
		logger.h \
		numbercleaner.h \
		fileutils.h \
		noncopyable.h \
		utf8_utils.h \
		ring_types.h \
		intrin.h \
		array_size.h \
		account_schema.h \
		registration_states.h \
		map_utils.h \
		string_utils.h \
		string_utils.cpp \
		rw_mutex.h \
		ring_api.cpp

nobase_include_HEADERS= dring/dring.h \
		dring/security_const.h \
		dring/callmanager_interface.h \
		dring/configurationmanager_interface.h \
		dring/presencemanager_interface.h \
		dring/account_const.h

if RING_VIDEO
nobase_include_HEADERS+= \
		dring/videomanager_interface.h
endif
