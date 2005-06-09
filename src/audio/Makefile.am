SUBDIRS = gsm

noinst_LTLIBRARIES = libaudio.la


libaudio_la_SOURCES = \
	alaw.cpp alaw.h \
	audiobuffer.cpp audiobuffer.h \
	audiocodec.cpp audiocodec.h \
	audiodrivers.cpp audiodrivers.h \
	audiodriversalsa.cpp audiodriversalsa.h \
	audiodriversoss.cpp audiodriversoss.h \
	audiodriversportaudio.cpp audiodriversportaudio.h \
	audiortp.cpp audiortp.h \
	codecDescriptor.cpp codecDescriptor.h \
	common.h \
	dtmf.cpp dtmf.h \
	dtmfgenerator.cpp dtmfgenerator.h \
	g711.cpp g711.h \
	gsmcodec.cpp gsmcodec.h \
	pa_converters.h pa_dither.h \
	tonegenerator.cpp tonegenerator.h \
	ulaw.cpp ulaw.h 

libaudio_la_LIBADD = gsm/libgsm.la

