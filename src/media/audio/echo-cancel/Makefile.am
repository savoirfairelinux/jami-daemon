noinst_LTLIBRARIES += libecho-cancel.la

EC_SRC = ./media/audio/echo-cancel/null_echo_canceller.cpp
EC_HDR = ./media/audio/echo-cancel/null_echo_canceller.h

if BUILD_SPEEXDSP
EC_SRC += ./media/audio/echo-cancel/speex_echo_canceller.cpp
EC_HDR += ./media/audio/echo-cancel/speex_echo_canceller.h
endif

if HAVE_WEBRTC_AP
EC_SRC += ./media/audio/echo-cancel/webrtc_echo_canceller.cpp
EC_HDR += ./media/audio/echo-cancel/webrtc_echo_canceller.h
libecho_cancel_la_CXXFLAGS = @WEBRTC_CFLAGS@ $(AM_CXXFLAGS)
endif

libecho_cancel_la_SOURCES = \
	$(EC_SRC)

noinst_HEADERS += \
	./media/audio/echo-cancel/echo_canceller.h \
	$(EC_HDR)

libaudio_la_LIBADD += libecho-cancel.la
