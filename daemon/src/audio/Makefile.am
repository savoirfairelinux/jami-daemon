include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libaudio.la

SUBDIRS = codecs audiortp sound

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

if BUILD_SPEEXDSP
SFL_SPEEXDSP_SRC=dsp.cpp
SFL_SPEEXDSP_HEAD=dsp.h
endif

libaudio_la_SOURCES = \
		audiobuffer.cpp \
		audioloop.cpp \
		ringbuffer.cpp \
		ringbufferpool.cpp \
		audiorecord.cpp \
		audiorecorder.cpp \
		recordable.cpp \
		audiolayer.cpp \
		resampler.cpp \
		$(SFL_SPEEXDSP_SRC) \
		dcblocker.cpp

noinst_HEADERS = \
		audiobuffer.h \
		audioloop.h \
		ringbuffer.h \
		ringbufferpool.h \
		audiorecord.h \
		audiorecorder.h \
		audiolayer.h \
	 	recordable.h \
		$(SFL_SPEEXDSP_HEAD) \
		dcblocker.h \
		resampler.h

libaudio_la_LIBADD = \
	./audiortp/libaudiortp.la \
	./codecs/libcodecdescriptor.la \
	./sound/libsound.la

if BUILD_PULSE
libaudio_la_LIBADD += ./pulseaudio/libpulselayer.la
endif

if BUILD_JACK
libaudio_la_LIBADD += ./jack/libjacklayer.la
libaudio_la_LDFLAGS = @JACK_LIBS@ -pthread
endif

if BUILD_ALSA
libaudio_la_LIBADD += ./alsa/libalsalayer.la
endif

if BUILD_OPENSL
libaudio_la_LIBADD += ./opensl/libopensl.la
libaudio_la_LDFLAGS = -lOpenSLES
endif
