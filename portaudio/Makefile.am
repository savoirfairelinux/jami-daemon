
noinst_LTLIBRARIES = libportaudio.la
libportaudio_la_CFLAGS = $(PORTAUDIO_CFLAGS) 
libportaudio_la_LIBADD = $(PORTAUDIO_LIBS)
libportaudio_la_CXXFLAGS = $(PORTAUDIO_CXXFLAGS)
libportaudio_la_CPPFLAGS = $(PORTAUDIO_CPPFLAGS) -I$(top_srcdir)/portaudio/pa_common/


if ENABLE_OSS
OSS=pa_unix_oss/pa_unix_oss.c
endif

if ENABLE_UNIX
UNIX=pa_unix/pa_unix_hostapis.c pa_unix/pa_unix_util.c
endif

if ENABLE_ALSA
ALSA=pa_linux_alsa/pa_linux_alsa.c
endif

SOURCES=$(OSS) $(UNIX) $(ALSA)

noinst_HEADERS = pa_common/portaudio.h \
pa_common/pa_allocation.h  pa_common/pa_dither.h      pa_common/pa_process.h  pa_common/pa_types.h  \
pa_common/pa_converters.h  pa_common/pa_endianness.h  pa_common/pa_stream.h   pa_common/pa_util.h   \
pa_common/pa_cpuload.h     pa_common/pa_hostapi.h     pa_common/pa_trace.h    pa_common/portaudio.h \
pa_unix/pa_unix_util.h pa_linux_alsa/pa_linux_alsa.h

libportaudio_la_SOURCES = $(SOURCES) \
	pa_common/pa_allocation.c \
	pa_common/pa_converters.c \
	pa_common/pa_cpuload.c \
	pa_common/pa_dither.c \
	pa_common/pa_front.c \
	pa_common/pa_process.c \
	pa_common/pa_skeleton.c \
	pa_common/pa_stream.c \
	pa_common/pa_trace.c

