include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libaudio.la

SUBDIRS = codecs audiortp sound alsa pulseaudio 

# if ENABLE_SPEEXDSP
# SPEEXDSP=-DHAVE_SPEEXDSP_LIB
# endif

libaudio_la_SOURCES = \
		audioloop.cpp \
		ringbuffer.cpp \
		mainbuffer.cpp \
		recordable.cpp \
		audiolayer.cpp \
		audiodevice.cpp \
		samplerateconverter.cpp \
		delaydetection.cpp \
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
		delaydetection.h \
		echocancel.h \
		speexechocancel.h \
		audioprocessing.h \
		dcblocker.h \
		samplerateconverter.h


