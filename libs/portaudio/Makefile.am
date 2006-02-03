noinst_LTLIBRARIES = libportaudio.la
libportaudio_la_CFLAGS = $(PORTAUDIO_CFLAGS) -Ipa_common/ 
libportaudio_la_LIBADD = $(PORTAUDIO) $(PORTAUDIO_LIBS)
libportaudio_la_CXXFLAGS = $(PORTAUDIO_CFLAGS)
libportaudio_la_CPPFLAGS = $(PORTAUDIO_CPPFLAGS)


if ENABLE_OSS
OSS=pa_unix_oss/pa_unix_oss.c
else 
OSS=
endif

if ENABLE_UNIX
UNIX=pa_unix/pa_unix_hostapis.c pa_unix/pa_unix_util.c
else
UNIX=
endif

if ENABLE_DARWIN
DARWIN=pa_mac/pa_mac_hostapis.c pa_unix/pa_unix_util.c pa_mac_core/pa_mac_core.c
else
DARWIN=
endif

if ENABLE_ALSA
ALSA=pa_linux_alsa/pa_linux_alsa.c
else
ALSA=
endif

if ENABLE_JACK
JACK=pa_jack/pa_jack.c pablio/ringbuffer.c
else
JACK=
endif

#if ENABLE_ASIO
#ASIO=pa_asio/iasiothiscallresolver.cpp $(ASIODIR)/host/asiodrivers.c $(ASIODIR)/common/asio.c $(ASIODIR)/host/mac/asioshlib.c
#else
ASIO=
#endif

PLATFORM_SOURCES=$(OSS) $(UNIX) $(ALSA) $(DARWIN) $(JACK) $(ASIO)

noinst_HEADERS = pa_common/portaudio.h \
pa_common/pa_allocation.h  pa_common/pa_dither.h      pa_common/pa_process.h  pa_common/pa_types.h  \
pa_common/pa_converters.h  pa_common/pa_endianness.h  pa_common/pa_stream.h   pa_common/pa_util.h   \
pa_common/pa_cpuload.h     pa_common/pa_hostapi.h     pa_common/pa_trace.h    pa_common/portaudio.h \
pa_unix/pa_unix_util.h pa_linux_alsa/pa_linux_alsa.h \
pablio/ringbuffer.h

libportaudio_la_SOURCES = $(PLATFORM_SOURCES) \
	pa_common/pa_allocation.c \
	pa_common/pa_converters.c \
	pa_common/pa_cpuload.c \
	pa_common/pa_dither.c \
	pa_common/pa_front.c \
	pa_common/pa_process.c \
	pa_common/pa_skeleton.c \
	pa_common/pa_stream.c \
	pa_common/pa_trace.c

