include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libfilters.la

libfilters_la_SOURCES = \
    media_processor.cpp \
    tensorflowsupervisedinference.cpp \
	multipleobjecttracking.cpp \
	reactive_streams_signals.cpp \
	ExVideoFrame.cpp

noinst_HEADERS = \
    media_processor.h \
    model.h \
    tensorflowsupervisedinference.h \
	multipleobjecttracking.h \
	asyncpublisher.h \
	asyncsubject.h \
	reactive_streams.h \
	reactive_streams_signals.h \
	ExVideoFrame.h


LIBS_DIR := /home/ayounes/Libs

AM_CPPFLAGS += -I$(LIBS_DIR)/flatbuffers -I$(LIBS_DIR)/tensorflow
libfilters_la_LDFLAGS = -L$(LIBS_DIR)/_tensorflow_dist_/lib/${ANDROID_ABI}  -ltensorflowlite -lopencv_core -lopencv_imgproc
