include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient.la

noinst_HEADERS = callmanager.h           \
                 configurationmanager.h

PRESENCE_SRC = presencemanager.cpp
noinst_HEADERS += presencemanager.h

if RING_VIDEO
VIDEO_SRC = videomanager.cpp
noinst_HEADERS += videomanager.h
endif

libclient_la_SOURCES = callmanager.cpp           \
                       configurationmanager.cpp  \
                       $(PRESENCE_SRC)           \
                       $(VIDEO_SRC)

libclient_la_CXXFLAGS = -I./ \
                        -I../ \
                        -DPREFIX=\"$(prefix)\" \
                        -DPROGSHAREDIR=\"${datadir}/ring\"
