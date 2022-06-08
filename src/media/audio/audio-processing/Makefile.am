noinst_LTLIBRARIES += libaudioprocessing.la

EC_SRC = ./media/audio/audio-processing/null_audio_processor.cpp
EC_HDR = ./media/audio/audio-processing/null_audio_processor.h

if BUILD_SPEEXDSP
EC_SRC += ./media/audio/audio-processing/speex.cpp
EC_HDR += ./media/audio/audio-processing/speex.h
endif

if HAVE_WEBRTC_AP
EC_SRC += ./media/audio/audio-processing/webrtc.cpp
EC_HDR += ./media/audio/audio-processing/webrtc.h
libaudioprocessing_la_CXXFLAGS = @WEBRTC_CFLAGS@ $(AM_CXXFLAGS)
endif

libaudioprocessing_la_SOURCES = \
	$(EC_SRC)

noinst_HEADERS += \
	./media/audio/audio-processing/audio_processor.h \
	$(EC_HDR)

libaudio_la_LIBADD += libaudioprocessing.la
