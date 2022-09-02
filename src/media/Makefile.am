noinst_LTLIBRARIES += libmedia.la

libmedia_la_LIBADD=

libmedia_la_SOURCES = \
	./media/libav_utils.cpp \
	./media/socket_pair.cpp \
	./media/media_buffer.cpp \
	./media/media_decoder.cpp \
	./media/media_encoder.cpp \
	./media/media_io_handle.cpp \
	./media/media_codec.cpp \
	./media/media_attribute.cpp \
	./media/system_codec_container.cpp \
	./media/srtp.c \
	./media/recordable.cpp \
	./media/media_filter.cpp \
	./media/media_recorder.cpp \
	./media/localrecorder.cpp \
	./media/media_player.cpp \
	./media/localrecordermanager.cpp \
	./media/congestion_control.cpp

noinst_HEADERS += \
	./media/rtp_session.h \
	./media/libav_utils.h \
	./media/libav_deps.h \
	./media/socket_pair.h \
	./media/media_buffer.h \
	./media/media_decoder.h \
	./media/media_encoder.h \
	./media/media_io_handle.h \
	./media/media_device.h \
	./media/media_codec.h \
	./media/system_codec_container.h \
	./media/srtp.h \
	./media/recordable.h \
	./media/decoder_finder.h \
	./media/media_filter.h \
	./media/media_stream.h \
	./media/media_recorder.h \
	./media/localrecorder.h \
	./media/media_player.h \
	./media/localrecordermanager.h \
	./media/congestion_control.h

include ./media/audio/Makefile.am
include ./media/video/Makefile.am

libjami_la_LIBADD += libmedia.la
