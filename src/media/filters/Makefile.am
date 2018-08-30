include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libfilters.la

libfilters_la_SOURCES = \
	media_processor.cpp

noinst_HEADERS = \
	media_processor.h

TF_BUILDDIR := /home/tmenais/tensorflow
AM_CPPFLAGS += -I/usr/local/lib/python3.6/dist-packages/tensorflow/include/  -I$(TF_BUILDDIR) -I$(TF_BUILDDIR)/bazel-genfiles/ 
libfilters_la_LDFLAGS = -L$(TF_BUILDDIR)/bazel-bin/tensorflow -ltensorflow_cc -ltensorflow_framework -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_objdetect -lopencv_videoio
