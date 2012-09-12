include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libaudio.la

SUBDIRS = codecs audiortp sound alsa

if BUILD_PULSE
SUBDIRS += pulseaudio
endif

if BUILD_SPEEXDSP
SFL_SPEEXDSP_SRC=noisesuppress.cpp
SFL_SPEEXDSP_HEAD=noisesuppress.h
endif

libaudio_la_SOURCES = \
		audioloop.cpp \
		ringbuffer.cpp \
		mainbuffer.cpp \
		audiorecord.cpp \
		audiorecorder.cpp \
		recordable.cpp \
		audiolayer.cpp \
		samplerateconverter.cpp \
		delaydetection.cpp \
		$(SFL_SPEEXDSP_SRC) \
		gaincontrol.cpp \
		dcblocker.cpp

noinst_HEADERS = \
		audioloop.h \
		ringbuffer.h \
		mainbuffer.h \
		audiorecord.h \
		audiorecorder.h \
		audiolayer.h \
	 	recordable.h \
		delaydetection.h \
		$(SFL_SPEEXDSP_HEAD) \
		gaincontrol.h \
		dcblocker.h \
		samplerateconverter.h

libaudio_la_LIBADD = \
	./audiortp/libaudiortp.la \
	./codecs/libcodecdescriptor.la \
	./alsa/libalsalayer.la \
	./sound/libsound.la

if BUILD_PULSE
libaudio_la_LIBADD += ./pulseaudio/libpulselayer.la
endif
