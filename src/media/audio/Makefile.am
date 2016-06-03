include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libaudio.la

SUBDIRS = sound

if BUILD_OPENSL
SUBDIRS += opensl
endif

if BUILD_ALSA
SUBDIRS += alsa
endif

if BUILD_PULSE
SUBDIRS += pulseaudio
endif

if BUILD_JACK
SUBDIRS += jack
endif

if HAVE_OSX
SUBDIRS += coreaudio
endif

if HAVE_IOS
SUBDIRS += coreaudio
endif

if HAVE_PORTAUDIO
SUBDIRS += portaudio
endif

if BUILD_SPEEXDSP
RING_SPEEXDSP_SRC=dsp.cpp
RING_SPEEXDSP_HEAD=dsp.h
endif

libaudio_la_SOURCES = \
		audiobuffer.cpp \
		audioloop.cpp \
		ringbuffer.cpp \
		ringbufferpool.cpp \
		audiorecord.cpp \
		audiorecorder.cpp \
		audiolayer.cpp \
		resampler.cpp \
		$(RING_SPEEXDSP_SRC) \
		dcblocker.cpp \
		audio_rtp_session.cpp \
		tonecontrol.cpp

libaudio_la_CXXFLAGS = -I$(top_srcdir)/src
libaudio_la_LDFLAGS =

if BUILD_SPEEXDSP
libaudio_la_CXXFLAGS += @SPEEXDSP_CFLAGS@
libaudio_la_LDFLAGS += @SPEEXDSP_LIBS@
endif

noinst_HEADERS = \
		audiobuffer.h \
		audioloop.h \
		ringbuffer.h \
		ringbufferpool.h \
		audiorecord.h \
		audiorecorder.h \
		audiolayer.h \
		$(RING_SPEEXDSP_HEAD) \
		dcblocker.h \
		resampler.h \
		audio_rtp_session.h \
		tonecontrol.h

libaudio_la_LIBADD = \
	./sound/libsound.la

if BUILD_PULSE
libaudio_la_LIBADD += ./pulseaudio/libpulselayer.la
endif

if BUILD_JACK
libaudio_la_LIBADD += ./jack/libjacklayer.la
libaudio_la_LDFLAGS += @JACK_LIBS@ -pthread
endif

if BUILD_ALSA
libaudio_la_LIBADD += ./alsa/libalsalayer.la
endif

if HAVE_OSX
libaudio_la_LIBADD += ./coreaudio/libcoreaudiolayer.la
endif

if HAVE_IOS
libaudio_la_LIBADD += ./coreaudio/libcoreaudiolayer.la
endif

if BUILD_OPENSL
libaudio_la_LIBADD += ./opensl/libopensl.la
libaudio_la_LDFLAGS += -lOpenSLES
endif

if BUILD_PORTAUDIO
libaudio_la_LIBADD += ./portaudio/libportaudiolayer.la
libaudio_la_LDFLAGS += @PORTAUDIO_LIBS@
endif
