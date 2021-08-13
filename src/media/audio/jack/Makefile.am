noinst_LTLIBRARIES += libjacklayer.la

libjacklayer_la_SOURCES = \
	./media/audio/jack/jacklayer.cpp \
	./media/audio/jack/jacklayer.h

libaudio_la_LDFLAGS += @JACK_LIBS@ -pthread
libaudio_la_LIBADD += libjacklayer.la
