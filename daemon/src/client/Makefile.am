include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient.la

noinst_HEADERS = callmanager.h           \
                 configurationmanager.h  \
                 client.h

if SFL_PRESENCE
PRESENCE_SRC = presencemanager.cpp
noinst_HEADERS += presencemanager.h
endif

if SFL_VIDEO
VIDEO_SRC = videomanager.cpp
noinst_HEADERS += videomanager.h
endif

libclient_la_SOURCES = client.cpp                \
                       callmanager.cpp           \
                       configurationmanager.cpp  \
                       $(PRESENCE_SRC)           \
                       $(VIDEO_SRC)

libclient_la_CXXFLAGS = -I./ \
                        -I../ \
                        -DPREFIX=\"$(prefix)\" \
                        -DPROGSHAREDIR=\"${datadir}/sflphone\"
