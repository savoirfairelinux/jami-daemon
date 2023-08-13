noinst_LTLIBRARIES += libaudio.la
libaudio_la_LIBADD=

libaudio_la_SOURCES = $(RING_SPEEXDSP_SRC) \
		./media/audio/audio_input.cpp \
		./media/audio/audio_frame_resizer.cpp \
		./media/audio/audioloop.cpp \
		./media/audio/ringbuffer.cpp \
		./media/audio/ringbufferpool.cpp \
		./media/audio/audiolayer.cpp \
		./media/audio/resampler.cpp \
		./media/audio/audio_sender.cpp \
		./media/audio/audio_receive_thread.cpp \
		./media/audio/audio_rtp_session.cpp \
		./media/audio/tonecontrol.cpp

libaudio_la_CXXFLAGS = \
	-I$(top_srcdir)/src \
	$(AM_CXXFLAGS)

libaudio_la_LDFLAGS = $(AM_LDFLAGS)

if BUILD_SPEEXDSP
libaudio_la_CXXFLAGS += @SPEEXDSP_CFLAGS@
libaudio_la_LDFLAGS += @SPEEXDSP_LIBS@
endif

if HAVE_WEBRTC_AP
libaudio_la_CXXFLAGS += @WEBRTC_CFLAGS@
endif

noinst_HEADERS += $(RING_SPEEXDSP_HEAD) \
		./media/audio/audio_input.h \
		./media/audio/audio_frame_resizer.h \
		./media/audio/audioloop.h \
		./media/audio/ringbuffer.h \
		./media/audio/ringbufferpool.h \
		./media/audio/audiolayer.h \
		./media/audio/resampler.h \
		./media/audio/audio_sender.h \
		./media/audio/audio_receive_thread.h \
		./media/audio/audio_rtp_session.h \
		./media/audio/tonecontrol.h


include ./media/audio/sound/Makefile.am
include ./media/audio/audio-processing/Makefile.am

if BUILD_OPENSL
include ./media/audio/opensl/Makefile.am
endif

if BUILD_ALSA
include ./media/audio/alsa/Makefile.am
endif

if BUILD_PULSE
include ./media/audio/pulseaudio/Makefile.am
endif

if BUILD_JACK
include ./media/audio/jack/Makefile.am
endif

if HAVE_OSX
include ./media/audio/coreaudio/Makefile.am
endif

if HAVE_IOS
include ./media/audio/coreaudio/Makefile.am
endif

if HAVE_PORTAUDIO
include ./media/audio/portaudio/Makefile.am
endif

libmedia_la_LIBADD += libaudio.la
