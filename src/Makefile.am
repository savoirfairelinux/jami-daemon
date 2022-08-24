include $(top_srcdir)/globals.mk

if HAVE_OSX
AM_OBJCXXFLAGS=-std=c++17
endif

noinst_LTLIBRARIES=
noinst_HEADERS=
nobase_include_HEADERS=

lib_LTLIBRARIES = libring.la
libring_la_LIBADD=

include ./client/Makefile.am
include ./media/Makefile.am
include ./config/Makefile.am
include ./sip/Makefile.am
include ./upnp/Makefile.am
include ./security/Makefile.am
include ./jamidht/Makefile.am
include ./im/Makefile.am
include ./plugin/Makefile.am

# libring

libring_la_LIBADD += \
	$(ALSA_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(PORTAUDIO_LIBS) \
	$(SPEEXDSP_LIBS) \
	$(PJPROJECT_LIBS) \
	$(OPENDHT_LIBS) \
	$(LIBUPNP_LIBS) \
	$(LIBGIT2_LIBS) \
	$(LIBAVCODEC_LIBS) \
	$(LIBAVFORMAT_LIBS) \
	$(LIBAVDEVICE_LIBS) \
	$(LIBAVFILTER_LIBS) \
	$(LIBSWRESAMPLE_LIBS) \
	$(LIBSWSCALE_LIBS) \
	$(LIBAVUTIL_LIBS) \
	$(WEBRTC_LIBS) \
	$(YAMLCPP_LIBS) \
	$(JSONCPP_LIBS) \
	$(NETTLE_LIBS) \
	$(SECP256K1_LIBS) \
	$(FMT_LIBS) \
	$(LIBSSL_LIBS) \
	$(LIBCRYPTO_LIBS) \
	$(ARCHIVE_LIBS) \
	$(ZLIB_LIBS) \
	$(BOOST_NOWIDE_LIBS)

if ENABLE_PLUGIN
if HAVE_OSX
libring_la_LIBADD += $(MINIZIP_LIBS)
else
libring_la_LIBADD += $(ARCHIVE_LIBS)
endif
endif

# XXX: It seems this should be the default value when otherwise left
# unspecified, but for some reason this is required otherwise the
# shared object fails to link against ffmpeg's libswscale.a.
libring_la_LDFLAGS = $(AM_LDFLAGS)
libring_la_LDFLAGS += -lnowide

if HAVE_WIN32
libring_la_LDFLAGS += -no-undefined -avoid-version
endif

if HAVE_OSX
#FIXME necessary for -lintl
libring_la_LDFLAGS += -L/usr/local/opt/gettext/lib
endif

libring_la_SOURCES = \
		buildinfo.cpp \
		conference.cpp \
		conference_protocol.cpp \
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
		vcard.cpp \
		ice_transport.cpp \
		ice_transport.h \
		threadloop.h \
		conference.h \
		conference_protocol.h \
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
		account_schema.h \
		registration_states.h \
		map_utils.h \
		string_utils.h \
		string_utils.cpp \
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
		uri.h \
		uri.cpp \
		vcard.h

if HAVE_WIN32
libring_la_SOURCES += \
		dlfcn.c \
		winsyslog.c
endif

nobase_include_HEADERS += \
	./jami/account_const.h \
	./jami/call_const.h \
	./jami/callmanager_interface.h \
	./jami/configurationmanager_interface.h \
	./jami/conversation_interface.h \
	./jami/datatransfer_interface.h \
	./jami/def.h \
	./jami/jami.h \
	./jami/media_const.h \
	./jami/presence_const.h \
	./jami/presencemanager_interface.h \
	./jami/security_const.h \
	./jami/trace-tools.h

if ENABLE_PLUGIN
nobase_include_HEADERS += \
		./jami/plugin_manager_interface.h
endif
if ENABLE_VIDEO
nobase_include_HEADERS += \
		./jami/videomanager_interface.h
endif

if ENABLE_TRACEPOINTS
libring_la_SOURCES += jami/tracepoint.h jami/tracepoint-def.h jami/tracepoint.c
libring_la_LIBADD  += $(LTTNG_LIBS)
endif
