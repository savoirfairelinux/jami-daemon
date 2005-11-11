SUBDIRS = gsm pacpp

noinst_LTLIBRARIES = libaudio.la


libaudio_la_SOURCES = alaw.cpp        audiofile.cpp	g711.cpp tonelist.cpp     \
audiortp.cpp         dtmf.cpp   tone.cpp audiocodec.cpp  audiolayer.cpp  dtmfgenerator.cpp  gsmcodec.cpp \
tonegenerator.cpp  ulaw.cpp codecDescriptor.cpp \
audioloop.cpp ringbuffer.cpp

AM_CXXFLAGS = -I$(top_srcdir)/include -I$(srcdir)/pacpp/include/ $(libccext2_CFLAGS) $(libccrtp1_CFLAGS) $(portaudio_CFLAGS) -I$(top_srcdir)/portaudio/pa_common
libaudio_la_LIBADD = gsm/libgsm.la pacpp/source/portaudiocpp/libportaudiocpp.la

noinst_HEADERS = audioloop.h       common.h ringbuffer.h alaw.h          audiofile.h g711.h \
 tonelist.h         audiortp.h audiocodec.h    audiolayer.h \
 dtmfgenerator.h    gsmcodec.h      tonegenerator.h    ulaw.h \
 audiodevice.h codecDescriptor.h    dtmf.h tone.h
