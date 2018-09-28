include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libfilters.la

libfilters_la_SOURCES = \
	media_processor.cpp

noinst_HEADERS = \
	media_processor.h
 
libfilters_la_LDFLAGS = `pkg-config --libs --cflags opencv` -lm
