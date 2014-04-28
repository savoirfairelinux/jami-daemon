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
noinst_HEADERS += video_controls.h
endif

SUBDIRS=dbus

libclient_la_LIBADD = dbus/libclient_dbus.la

libclient_la_SOURCES = callmanager.cpp           \
                       configurationmanager.cpp  \
                       $(PRESENCE_SRC)

libclient_la_CXXFLAGS = -I./ \
                        -DPREFIX=\"$(prefix)\" \
                        -DPROGSHAREDIR=\"${datadir}/sflphone\"
