noinst_LTLIBRARIES += libportaudiolayer.la

libportaudiolayer_la_SOURCES = \
		./media/audio/portaudio/portaudiolayer.cpp


noinst_HEADERS += \
		./media/audio/portaudio/portaudiolayer.h

libaudio_la_LDFLAGS += @PORTAUDIO_LIBS@
libaudio_la_LIBADD += libportaudiolayer.la
