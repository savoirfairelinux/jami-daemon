if MAINTENER_CODE
noinst_PROGRAMS = example01 example02 example03 example04 example05
noinst_LTLIBRARIES = libsflaudio.la

libsflaudio_la_SOURCES = \
	AudioLayer.cpp AudioLayer.hpp \
	AudioManagerImpl.cpp AudioManagerImpl.hpp \
	Context.cpp Context.hpp \
	Device.cpp Device.hpp \
	Emitter.cpp Emitter.hpp \
	MicEmitter.cpp MicEmitter.hpp \
	NullLayer.cpp NullLayer.hpp \
	NullContext.cpp NullContext.hpp \
	NullDevice.cpp NullDevice.hpp \
	NullEmitter.hpp \
	NullSource.cpp NullSource.hpp \
	OpenALLayer.cpp OpenALLayer.hpp \
	OpenALDevice.cpp OpenALDevice.hpp \
	OpenALContext.cpp OpenALContext.hpp \
	OpenALSource.cpp OpenALSource.hpp \
	PortAudioLayer.cpp PortAudioLayer.hpp \
	SFLAudio.hpp \
	Source.cpp Source.hpp \
	WavEmitter.cpp WavEmitter.hpp

AM_CPPFLAGS = -I$(top_srcdir)/libs/portaudio/pa_common -I$(top_srcdir)/libs/
AM_CXXFLAGS = $(PORTAUDIO_CXXFLAGS) $(PORTAUDIO_CFLAGS) -I$(top_srcdir)/libs/ $(libccext2_CFLAGS) $(libccgnu2_CFLAGS)
AM_LDFLAGS  =  -L$(top_srcdir)/libs/portaudio/ 

LIBADD = $(PORTAUDIO_LIBS) -lportaudio -lopenal -lalut
LDADD = libsflaudio.la $(PORTAUDIO_LIBS) -lopenal -lportaudio -lalut $(libccgnu2_LIBS)


example01_SOURCES = example01.cpp
example02_SOURCES = example02.cpp
example03_SOURCES = example03.cpp
example04_SOURCES = example04.cpp
example05_SOURCES = example05.cpp 
endif

