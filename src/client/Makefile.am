noinst_LTLIBRARIES += libclient.la

noinst_HEADERS += \
	./client/ring_signal.h

PRESENCE_SRC = ./client/presencemanager.cpp

VIDEO_SRC = ./client/videomanager.cpp
noinst_HEADERS += ./client/videomanager.h

if ENABLE_PLUGIN
PLUGIN_SRC = ./client/plugin_manager_interface.cpp
endif

libclient_la_SOURCES = \
	./client/ring_signal.cpp \
	./client/callmanager.cpp \
	./client/configurationmanager.cpp \
	./client/datatransfer.cpp \
	./client/conversation_interface.cpp \
	$(PLUGIN_SRC) \
	$(PRESENCE_SRC) \
	$(VIDEO_SRC)

libjami_la_LIBADD += libclient.la
