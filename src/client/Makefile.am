include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libclient.la

noinst_HEADERS = \
	ring_signal.h

PRESENCE_SRC = presencemanager.cpp

if ENABLE_VIDEO
VIDEO_SRC = videomanager.cpp
noinst_HEADERS += videomanager.h
endif

if ENABLE_PLUGIN
PLUGIN_SRC = plugin_manager_interface.cpp
endif

libclient_la_SOURCES = \
	ring_signal.cpp \
	callmanager.cpp \
	configurationmanager.cpp \
	datatransfer.cpp \
	conversation_interface.cpp \
	$(PLUGIN_SRC) \
	$(PRESENCE_SRC) \
	$(VIDEO_SRC)
