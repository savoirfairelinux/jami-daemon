include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libaudio.la

SUBDIRS = codecs audiortp sound alsa pulseaudio

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
		echosuppress.cpp \
		speexechocancel.cpp \
		noisesuppress.cpp \
		gaincontrol.cpp \
		dcblocker.cpp

noinst_HEADERS = \
		audioloop.h \
		common.h \
		ringbuffer.h \
		mainbuffer.h \
		audiorecord.h \
		audiorecorder.h \
		audiolayer.h \
	 	recordable.h \
		delaydetection.h \
		echosuppress.h \
		speexechocancel.h \
		noisesuppress.h \
		gaincontrol.h \
		dcblocker.h \
		samplerateconverter.h

libaudio_la_LIBADD = \
	./audiortp/libaudiortp.la \
	./codecs/libcodecdescriptor.la \
	./alsa/libalsalayer.la \
	./pulseaudio/libpulselayer.la \
	./sound/libsound.la



