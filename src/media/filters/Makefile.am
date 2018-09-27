include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libfilters.la

libfilters_la_SOURCES = \
	media_processor.cpp

noinst_HEADERS = \
	media_processor.h

TF_BUILDDIR := /home/tmenais/tensorflow
AM_CPPFLAGS += -I
libfilters_la_LDFLAGS = -L`pkg-config --libs --cflags opencv` -lm
