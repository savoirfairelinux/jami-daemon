include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libecho-cancel.la

EC_SRC = null_echo_canceller.cpp
EC_HDR = null_echo_canceller.h

if BUILD_SPEEXDSP
EC_SRC += speex_echo_canceller.cpp
EC_HDR += speex_echo_canceller.h
endif

if HAVE_WEBRTC_AP
EC_SRC += webrtc_echo_canceller.cpp
EC_HDR += webrtc_echo_canceller.h
libecho_cancel_la_CXXFLAGS = @WEBRTC_CFLAGS@ $(AM_CXXFLAGS)
endif

libecho_cancel_la_SOURCES = \
	$(EC_SRC)

noinst_HEADERS = \
	echo_canceller.h \
	$(EC_HDR)
