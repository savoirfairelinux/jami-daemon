noinst_LTLIBRARIES += libopensl.la

libopensl_la_SOURCES = \
	./media/audio/opensl/opensllayer.cpp \
	./media/audio/opensl/audio_player.cpp \
	./media/audio/opensl/audio_recorder.cpp

noinst_HEADERS += \
	./media/audio/opensl/opensllayer.h \
	./media/audio/opensl/audio_common.h \
	./media/audio/opensl/audio_player.h \
	./media/audio/opensl/audio_recorder.h \
	./media/audio/opensl/buf_manager.h


libaudio_la_LDFLAGS += -lOpenSLES
libaudio_la_LIBADD += libopensl.la
