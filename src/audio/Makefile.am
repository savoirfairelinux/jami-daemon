sflcodecdir = $(libdir)/sflphone/codecs/

noinst_LTLIBRARIES = libaudio.la
noinst_PROGRAMS = libcodec_ulaw.so libcodec_alaw.so libcodec_gsm.so libcodec_speex.so

libcodec_ulaw_so_SOURCES = ulaw.cpp
libcodec_ulaw_so_CFLAGS = -fPIC -g -Wall 
libcodec_ulaw_so_LDFLAGS = -shared -lc

libcodec_alaw_so_SOURCES = alaw.cpp
libcodec_alaw_so_CFLAGS = -fPIC -g -Wall 
libcodec_alaw_so_LDFLAGS = -shared -lc

libcodec_gsm_so_SOURCES = gsmcodec.cpp
libcodec_gsm_so_CFLAGS = -fPIC -g -Wall 
libcodec_gsm_so_LDFLAGS = -shared -lc -lgsm

libcodec_speex_so_SOURCES = CodecSpeex.cpp
libcodec_speex_so_CFLAGS = -fPIC -g -Wall 
libcodec_speex_so_LDFLAGS = -shared -lc -lspeex

if USE_SPEEX
SPEEX_SOURCES_CPP=CodecSpeex.cpp
SPEEX_FLAG=-DHAVE_SPEEX
SPEEX_LIB=-lspeex
else
SPEEX_SOURCES_CPP=
SPEEX_FLAG=
SPEEX_LIB=
endif

#SUBDIRS = ilbc
 
libaudio_la_SOURCES =      audiofile.cpp	 tonelist.cpp     \
audiortp.cpp         dtmf.cpp   tone.cpp  audiolayer.cpp audiodevice.cpp  dtmfgenerator.cpp   \
tonegenerator.cpp   codecDescriptor.cpp \
audioloop.cpp ringbuffer.cpp $(SPEEX_SOURCES_CPP)

AM_CXXFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs $(libccext2_CFLAGS) $(libdbuscpp_CFLAGS) $(libccrtp1_CFLAGS) $(USER_INCLUDES) \
 	      -DCODECS_DIR=\""$(sflcodecdir)"\"	
libaudio_la_CPPFLAGS = $(SPEEX_FLAG)

noinst_HEADERS = audioloop.h       common.h ringbuffer.h           audiofile.h  \
 tonelist.h         audiortp.h audiocodec.h    audiolayer.h audiodevice.h \
 dtmfgenerator.h         tonegenerator.h     \
 codecDescriptor.h    dtmf.h tone.h \
 CodecSpeex.h

install-exec-local:	install-libcodec_ulaw_so install-libcodec_alaw_so install-libcodec_gsm_so install-libcodec_speex_so
uninstall-local:  uninstall-libcodec_ulaw_so uninstall-libcodec_alaw_so uninstall-libcodec_gsm_so uninstall-libcodec_speex_so

install-libcodec_ulaw_so: libcodec_ulaw.so
	mkdir -p $(sflcodecdir)
	$(INSTALL_PROGRAM)  libcodec_ulaw.so $(sflcodecdir)
install-libcodec_alaw_so: libcodec_alaw.so
	$(INSTALL_PROGRAM)  libcodec_alaw.so $(sflcodecdir)
install-libcodec_gsm_so: libcodec_gsm.so
	$(INSTALL_PROGRAM)  libcodec_gsm.so $(sflcodecdir)
install-libcodec_speex_so: libcodec_speex.so
	$(INSTALL_PROGRAM)  libcodec_speex.so $(sflcodecdir)

uninstall-libcodec_ulaw_so:
	rm -f $(sflcodecdir)/libcodec_ulaw.so
uninstall-libcodec_alaw_so:
	rm -f $(sflcodecdir)/libcodec_alaw.so
uninstall-libcodec_gsm_so:
	rm -f $(sflcodecdir)/libcodec_gsm.so  
uninstall-libcodec_speex_so:
	rm -f $(sflcodecdir)/libcodec_speex.so  
	rm -rf $(sflcodecdir) 
