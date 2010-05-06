include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libaudio.la

SUBDIRS = codecs audiortp sound alsa pulseaudio 

libaudio_la_SOURCES = \
		audioloop.cpp \
		ringbuffer.cpp \
		mainbuffer.cpp \
		recordable.cpp \
		audiolayer.cpp \
		audiodevice.cpp \
		samplerateconverter.cpp \
		echocancel.cpp \
		speexechocancel.cpp \
		audioprocessing.cpp \
		dcblocker.cpp \
		$(SPEEX_SOURCES_CPP)

noinst_HEADERS = \
		audioloop.h \
		common.h \
		ringbuffer.h \
		audiolayer.h \
		audiodevice.h \
		mainbuffer.h \
	 	recordable.h \
		algorithm.h \
		echocancel.h
		speexechocancel.h \
		audioprocessing.h \
		dcblocker.h \
		samplerateconverter.h


