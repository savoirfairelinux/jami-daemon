include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient.la

noinst_HEADERS = callmanager.h           \
                 configurationmanager.h  \
                 client.h

if SFL_PRESENCE
PRESENCE_SRC = presencemanager.cpp
PRESENCE_STUB = presencemanager_stub.cpp
noinst_HEADERS += presencemanager.h
endif

if SFL_VIDEO
VIDEO_SRC = videomanager.cpp
VIDEO_STUB = videomanager_stub.cpp
noinst_HEADERS += videomanager.h
endif

if USE_DBUS
SUBDIRS=dbus

libclient_la_LIBADD = dbus/libclient_dbus.la
else
STUB_SRC = client.cpp                      \
           callmanager_stub.cpp            \
           configurationmanager_stub.cpp   \
           $(PRESENCE_STUB)                \
           $(VIDEO_STUB)
endif

libclient_la_SOURCES = callmanager.cpp           \
                       configurationmanager.cpp  \
                       $(PRESENCE_SRC)           \
                       $(VIDEO_SRC)              \
                       $(STUB_SRC)

libclient_la_CXXFLAGS = -I./ \
                        -DPREFIX=\"$(prefix)\" \
                        -DPROGSHAREDIR=\"${datadir}/sflphone\"
