include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libaudiortp.la


libaudiortp_la_SOURCES = \
		avformat_rtp_session.cpp \
		avformat_rtp_session.h

AM_CXXFLAGS = @LIBAVFORMAT_CFLAGS@

# FIXME
AM_CPPFLAGS += -I$(top_srcdir)/src
