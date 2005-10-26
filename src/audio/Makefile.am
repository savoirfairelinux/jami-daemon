SUBDIRS = gsm pacpp

noinst_LTLIBRARIES = libaudio.la


libaudio_la_SOURCES = alaw.cpp alaw.h 	audiocodec.cpp audiocodec.h \
		audiolayer.cpp audiolayer.h 	audiortp.cpp audiortp.h 	codecDescriptor.cpp \
	codecDescriptor.h 	common.h 	dtmf.cpp dtmf.h 	dtmfgenerator.cpp dtmfgenerator.h 	g711.cpp \
	g711.h 	gsmcodec.cpp gsmcodec.h 	ringbuffer.cpp ringbuffer.h \
		tonegenerator.cpp tonegenerator.h 	ulaw.cpp ulaw.h tone.cpp tonelist.cpp

AM_CXXFLAGS = -I$(top_srcdir) -I$(srcdir)/pacpp/include/ $(libccext2_CFLAGS) $(libccrtp1_CFLAGS) $(portaudio_CFLAGS)
libaudio_la_LIBADD = gsm/libgsm.la pacpp/source/portaudiocpp/libportaudiocpp.la

noinst_HEADERS = tone.h tonelist.h
