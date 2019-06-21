include $(top_srcdir)/globals.mk

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

libaudio_la_SOURCES = \
		audiobuffer.cpp \
		audio_input.cpp \
		audio_frame_resizer.cpp \
		audioloop.cpp \
		ringbuffer.cpp \
		ringbufferpool.cpp \
		audiolayer.cpp \
		resampler.cpp \
		dcblocker.cpp \
		audio_sender.cpp \
		audio_receive_thread.cpp \
		audio_rtp_session.cpp \
		tonecontrol.cpp

libaudio_la_CXXFLAGS = -I$(top_srcdir)/src
libaudio_la_LDFLAGS =

noinst_HEADERS = \
		audiobuffer.h \
		audio_input.h \
		audio_frame_resizer.h \
		audioloop.h \
		ringbuffer.h \
		ringbufferpool.h \
		audiolayer.h \
		resampler.h \
		dcblocker.h \
		audio_sender.h \
		audio_receive_thread.h \
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
