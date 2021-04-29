include ../globals.mk

ENABLE_PLUGIN_LIBS=

if ENABLE_PLUGIN
ENABLE_PLUGIN_LIBS+=./plugin/libplugin.la
endif

ENABLE_VIDEO_LIBS=

if ENABLE_VIDEO
ENABLE_VIDEO_LIBS+=./media/video/libvideo.la
if HAVE_LINUX
if HAVE_ANDROID
ENABLE_VIDEO_LIBS+= \
                ./media/video/androidvideo/libandroidvideo.la
else
ENABLE_VIDEO_LIBS+= \
                ./media/video/v4l2/libv4l2.la
endif
endif
if HAVE_OSX
ENABLE_VIDEO_LIBS+= \
                ./media/video/osxvideo/libosxvideo.la
endif
if HAVE_IOS
ENABLE_VIDEO_LIBS+= \
                ./media/video/iosvideo/libiosvideo.la
endif
if HAVE_WIN32
ENABLE_VIDEO_LIBS+= \
                ./media/video/winvideo/libwinvideo.la
endif
endif

SUBDIRS = client media config sip upnp security jamidht im $(ENABLE_VIDEO_SUBDIR)

if ENABLE_PLUGIN
SUBDIRS+=plugin
endif

# libring

lib_LTLIBRARIES = libring.la

libring_la_LIBADD = \
	./sip/libsiplink.la \
	./media/libmedia.la \
	./client/libclient.la \
	./config/libconfig.la \
	./security/libsecurity.la \
	./upnp/libupnpcontrol.la \
	./jamidht/libringacc.la \
	./im/libim.la \
	$(ENABLE_PLUGIN_LIBS) \
	$(ENABLE_VIDEO_LIBS)

libring_la_LDFLAGS = \
		@PJPROJECT_LIBS@ \
		@ALSA_LIBS@ \
		@PULSEAUDIO_LIBS@ \
		@YAMLCPP_LIBS@ \
		@JSONCPP_LIBS@ \
		@SPEEXDSP_LIBS@ \
		@LIBUPNP_LIBS@ \
		@PORTAUDIO_LIBS@ \
		@GNUTLS_LIBS@ \
		@OPENDHT_LIBS@ \
		@SECP256K1_LIBS@ \
		@ZLIB_LIBS@ \
		@LIBSSL_LIBS@ \
		@LIBCRYPTO_LIBS@ \
		@LIBAVCODEC_LIBS@ \
		@LIBAVFORMAT_LIBS@ \
		@LIBAVDEVICE_LIBS@ \
		@LIBAVFILTER_LIBS@ \
		@LIBSWRESAMPLE_LIBS@ \
		@LIBSWSCALE_LIBS@ \
		@LIBAVUTIL_LIBS@ \
		@LIBGIT2_LIBS@ \
		@FMT_LIBS@ \
		@LIBS@ \
		@WEBRTC_LIBS@ \
		$(AM_LDFLAGS)

if ENABLE_PLUGIN
libring_la_LDFLAGS+= \
		@MINIZIP_LIBS@
endif


if HAVE_WIN32
libring_la_LDFLAGS += -no-undefined -avoid-version
endif

if HAVE_OSX
#FIXME necessary for -lintl
libring_la_LDFLAGS += -L/usr/local/opt/gettext/lib
endif

libring_la_CFLAGS = \
		@PJPROJECT_CFLAGS@ \
		@ALSA_CFLAGS@ \
		@PULSEAUDIO_CFLAGS@ \
		@LIBUPNP_CFLAGS@ \
		@SPEEXDSP_CFLAGS@ \
		@PORTAUDIO_CFLAGS@ \
		@GNUTLS_CFLAGS@ \
		@OPENDHT_CFLAGS@ \
		@FMT_CFLAGS@ \
		@LIBAVCODEC_CFLAGS@ \
		@LIBAVFORMAT_CFLAGS@ \
		@LIBAVUTIL_CFLAGS@ \
		@LIBAVDEVICE_CFLAGS@ \
		@LIBAVFILTER_CFLAGS@ \
		@LIBSWRESAMPLE_CFLAGS@ \
		@LIBSWSCALE_CFLAGS@ \
		@LIBGIT2_CFLAGS@ \
		@WEBRTC_CFLAGS@ \
		$(AM_CFLAGS)

libring_la_CXXFLAGS = $(AM_CXXFLAGS)

if HAVE_LINUX
#needed to compile the .so
libring_la_CXXFLAGS += -fPIC
endif

libring_la_SOURCES = \
		buildinfo.cpp \
		conference.cpp \
		account_factory.cpp \
		call_factory.cpp \
		preferences.cpp \
		manager.cpp \
		call.cpp \
		gittransport.cpp \
		account.cpp \
		logger.cpp \
		fileutils.cpp \
		archiver.cpp \
		threadloop.cpp \
		ip_utils.h \
		ip_utils.cpp \
		utf8_utils.cpp \
		ice_transport.cpp \
		ice_transport.h \
		threadloop.h \
		conference.h \
		account_factory.h \
		call_factory.h \
		preferences.h \
		manager.h \
		gittransport.h \
		account.h \
		call.h \
		logger.h \
		fileutils.h \
		archiver.h \
		noncopyable.h \
		utf8_utils.h \
		ring_types.h \
		compiler_intrinsics.h \
		array_size.h \
		account_schema.h \
		registration_states.h \
		map_utils.h \
		string_utils.h \
		string_utils.cpp \
		rw_mutex.h \
		ring_api.cpp \
		rational.h \
		smartools.cpp \
		smartools.h \
		base64.h \
		base64.cpp \
		peer_connection.cpp \
		peer_connection.h \
		data_transfer.cpp \
		data_transfer.h \
		ftp_server.cpp \
		ftp_server.h \
		generic_io.h \
		scheduled_executor.h \
		scheduled_executor.cpp \
		transport/peer_channel.h \
		vcard.h

if HAVE_WIN32
libring_la_SOURCES += \
		dlfcn.c \
		winsyslog.c
endif

nobase_include_HEADERS= dring/dring.h \
		dring/security_const.h \
		dring/callmanager_interface.h \
		dring/configurationmanager_interface.h \
		dring/presencemanager_interface.h \
		dring/datatransfer_interface.h \
		dring/account_const.h \
		dring/call_const.h \
		dring/presence_const.h \
		dring/media_const.h \
		dring/def.h

if ENABLE_PLUGIN
nobase_include_HEADERS+= \
		dring/plugin_manager_interface.h
endif
if ENABLE_VIDEO
nobase_include_HEADERS+= \
		dring/videomanager_interface.h
endif
