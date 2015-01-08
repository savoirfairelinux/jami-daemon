include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libaudiortp.la

if BUILD_ZRTP
RING_ZRTP_SRC=audio_zrtp_session.h audio_zrtp_session.cpp zrtp_session_callback.cpp zrtp_session_callback.h
endif

libaudiortp_la_SOURCES = \
		$(RING_ZRTP_SRC) \
		base64.c base64.h

if RING_VIDEO
libaudiortp_la_SOURCES += \
		avformat_rtp_session.cpp \
		avformat_rtp_session.h

AM_CXXFLAGS = @LIBAVFORMAT_CFLAGS@

else
libaudiortp_la_SOURCES += \
		audio_rtp_session.cpp \
		audio_symmetric_rtp_session.cpp \
		audio_rtp_stream.cpp \
		audio_rtp_factory.cpp \
		audio_srtp_session.cpp \
		audio_rtp_session.h \
		dtmf_event.h \
		dtmf_event.cpp \
		audio_rtp_stream.h \
		audio_rtp_factory.h \
		audio_symmetric_rtp_session.h \
		audio_srtp_session.h
endif

# FIXME
AM_CPPFLAGS += -I$(top_srcdir)/src
