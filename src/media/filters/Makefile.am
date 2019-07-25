include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libfilters.la

libfilters_la_SOURCES = \
        media_processor.cpp \
        tensorflowsupervisedinference.cpp \
        multipleobjecttracking.cpp

noinst_HEADERS = \
        media_processor.h \
        framelistener.h \
        model.h \
        tensorflowsupervisedinference.h \
        multipleobjecttracking.h


LIBS_DIR := /home/ayounes/Libs

AM_CPPFLAGS += -I$(LIBS_DIR)/flatbuffers -I$(LIBS_DIR)/tensorflow
libfilters_la_LDFLAGS = -L/home/ayounes/Libs/_tensorflow_dist_/lib/${JAMI_PLATFORM}  -ltensorflowlite -lopencv_core -lopencv_imgproc -lopencv_highgui
