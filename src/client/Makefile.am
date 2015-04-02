include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient.la

noinst_HEADERS = \
	ring_signal.h

PRESENCE_SRC = presencemanager.cpp

if RING_VIDEO
VIDEO_SRC = videomanager.cpp
noinst_HEADERS += videomanager.h
endif

libclient_la_SOURCES = \
	ring_signal.cpp \
	callmanager.cpp \
	configurationmanager.cpp \
	$(PRESENCE_SRC) \
	$(VIDEO_SRC)

libclient_la_CXXFLAGS = \
	-I./ \
	-I../ \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/ring\"
