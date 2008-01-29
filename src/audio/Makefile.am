#SUBDIRS = gsm 

noinst_LTLIBRARIES = libaudio.la

if USE_SPEEX
SPEEX_SOURCES_CPP=CodecSpeex.cpp
SPEEX_FLAG=-DHAVE_SPEEX
SPEEX_LIB=-lspeex
else
SPEEX_SOURCES_CPP=
SPEEX_FLAG=
SPEEX_LIB=
endif

libaudio_la_SOURCES =      audiofile.cpp	 tonelist.cpp     \
audiortp.cpp         dtmf.cpp   tone.cpp  audiolayer.cpp audiodevice.cpp  dtmfgenerator.cpp   \
tonegenerator.cpp   codecDescriptor.cpp \
audioloop.cpp ringbuffer.cpp $(SPEEX_SOURCES_CPP)

AM_CXXFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs $(libccext2_CFLAGS) $(libdbuscpp_CFLAGS) $(libccrtp1_CFLAGS) $(USER_INCLUDES) 
#libaudio_la_LIBADD = gsm/libgsm.la $(SPEEX_LIB)
libaudio_la_CPPFLAGS = $(SPEEX_FLAG)


noinst_HEADERS = audioloop.h       common.h ringbuffer.h           audiofile.h  \
 tonelist.h         audiortp.h audiocodec.h    audiolayer.h audiodevice.h \
 dtmfgenerator.h         tonegenerator.h     \
 codecDescriptor.h    dtmf.h tone.h \
 CodecSpeex.h
