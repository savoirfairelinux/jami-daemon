sflcodecdir = $(libdir)/sflphone/codecs

noinst_LTLIBRARIES = libaudio.la

if BUILD_SPEEX
SPEEX_FLAG = -DBUILD_SPEEX
else
SPEEX_FLAG = 
endif

if BUILD_GSM
GSM_FLAG = -DBUILD_GSM
else
GSM_FLAG = 
endif

SUBDIRS = codecs

libaudio_la_SOURCES =    audiofile.cpp	 tonelist.cpp    \
audiortp.cpp         dtmf.cpp   tone.cpp  audiolayer.cpp audiodevice.cpp  dtmfgenerator.cpp   \
tonegenerator.cpp   codecDescriptor.cpp \
audioloop.cpp ringbuffer.cpp $(SPEEX_SOURCES_CPP)

AM_CXXFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs $(libccext2_CFLAGS) $(libdbuscpp_CFLAGS) $(libccrtp1_CFLAGS) $(USER_INCLUDES) \
 	      -DCODECS_DIR=\""$(sflcodecdir)"\" $(SPEEX_FLAG) $(GSM_FLAG)	

noinst_HEADERS = audioloop.h    common.h ringbuffer.h    audiofile.h  \
 tonelist.h         audiortp.h   audiolayer.h audiodevice.h \
 dtmfgenerator.h         tonegenerator.h     \
 codecDescriptor.h    dtmf.h tone.h 

