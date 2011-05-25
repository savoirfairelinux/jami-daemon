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
		audiorecord.cpp \
		audiorecorder.cpp \
		recordable.cpp \
		audiolayer.cpp \
		audiodevice.cpp \
		samplerateconverter.cpp \
		delaydetection.cpp \
		echocancel.cpp \
		echosuppress.cpp \
		speexechocancel.cpp \
		noisesuppress.cpp \
		audioprocessing.cpp \
		gaincontrol.cpp \
		dcblocker.cpp \
		$(SPEEX_SOURCES_CPP)

noinst_HEADERS = \
		audioloop.h \
		common.h \
		ringbuffer.h \
		mainbuffer.h \
		audiorecord.h \
		audiorecorder.h \
		audiolayer.h \
		audiodevice.h \
	 	recordable.h \
		algorithm.h \
		delaydetection.h \
		echocancel.h \
		echosuppress.h \
		speexechocancel.h \
		noisesuppress.h \
		audioprocessing.h \
		gaincontrol.h \
		dcblocker.h \
		samplerateconverter.h

libaudio_la_LIBADD = \
	./audiortp/libaudiortp.la \
	./codecs/libcodecdescriptor.la \
	./alsa/libalsalayer.la \
	./pulseaudio/libpulselayer.la \
	./sound/libsound.la 



