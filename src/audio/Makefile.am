include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libaudio.la


if BUILD_ILBC
ILBC_FLAG = -DBUILD_ILBC
else
ILBC_FLAG = 
endif

SUBDIRS = codecs

libaudio_la_SOURCES = \
		audiofile.cpp \
		tonelist.cpp \
		audiortp.cpp \
		audiostream.cpp \
		dtmf.cpp \
		tone.cpp \
		alsalayer.cpp \
		audiolayer.cpp \
		pulselayer.cpp \
		audiodevice.cpp \
		dtmfgenerator.cpp \
		tonegenerator.cpp \
		codecDescriptor.cpp \
		audioloop.cpp \
		ringbuffer.cpp \
		$(SPEEX_SOURCES_CPP)

noinst_HEADERS = \
		audioloop.h \
		audiostream.h \
		common.h \
		ringbuffer.h \
		audiofile.h \
	 	tonelist.h \
		audiortp.h \
		audiolayer.h \
		alsalayer.h \
		pulselayer.h \
		audiodevice.h \
 		dtmfgenerator.h \
		tonegenerator.h \
 		codecDescriptor.h \
		dtmf.h \
		tone.h 

