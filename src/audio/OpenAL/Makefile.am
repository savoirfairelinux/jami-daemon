if MAINTENER_CODE
noinst_PROGRAMS = example01 example02
noinst_LTLIBRARIES = libsflaudio.la

libsflaudio_la_SOURCES = \
	AudioLayer.cpp AudioLayer.hpp \
	AudioManagerImpl.cpp AudioManagerImpl.hpp \
	Context.cpp Context.hpp \
	Device.cpp Device.hpp \
	NullLayer.cpp NullLayer.hpp \
	NullDevice.cpp NullDevice.hpp \
	NullContext.cpp NullContext.hpp \
	NullSource.hpp \
	OpenALLayer.cpp OpenALLayer.hpp \
	OpenALDevice.cpp OpenALDevice.hpp \
	PortAudioLayer.cpp PortAudioLayer.hpp \
	SFLAudio.hpp \
	Source.hpp 

AM_CPPFLAGS = $(PORTAUDIO_CFLAGS) -I$(top_srcdir)/libs/portaudio/pa_common -I$(top_srcdir)/libs/
AM_CXXFLAGS = $(PORTAUDIO_CXXFLAGS) -I$(top_srcdir)/libs/
AM_LDFLAGS  =  -L$(top_srcdir)/libs/portaudio/ 

LIBADD = $(PORTAUDIO_LIBS) -lportaudio -lopenal
LDADD = libsflaudio.la $(PORTAUDIO_LIBS) -lopenal -lportaudio


example01_SOURCES = example01.cpp
example02_SOURCES = example02.cpp
endif

