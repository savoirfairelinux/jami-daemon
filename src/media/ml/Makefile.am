include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libml.la

libml_la_SOURCES = \
        testframeinput.cpp \
        framelistener.cpp \
        tensorflowsupervisedinference.cpp \
        multipleobjecttracking.cpp \
        frameresizer.cpp

noinst_HEADERS = \
        testframeinput.h \
        framelistener.h \
        model.h \
        tensorflowsupervisedinference.h \
        multipleobjecttracking.h \
        frameresizer.h

LIBS_DIR := /home/ayounes/Libs

AM_CPPFLAGS += -I$(LIBS_DIR)/flatbuffers -I$(LIBS_DIR)/tensorflow -I$(LIBS_DIR)/CImg/
libml_la_LDFLAGS = -L$(LIBS_DIR)/tensorflow/bazel-bin/tensorflow/lite -ltensorflowlite -L/usr/X11R6/lib -lm -lpthread -lX11
