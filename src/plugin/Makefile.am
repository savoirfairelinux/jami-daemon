if ENABLE_PLUGIN

noinst_LTLIBRARIES += libplugin.la

noinst_HEADERS += \
	./plugin/preferenceservicesmanager.h \
	./plugin/callservicesmanager.h \
	./plugin/preferencehandler.h \
	./plugin/chathandler.h \
	./plugin/chatservicesmanager.h \
	./plugin/webviewservicesmanager.h \
	./plugin/webviewhandler.h \
	./plugin/jamiplugin.h \
	./plugin/jamipluginmanager.h \
	./plugin/mediahandler.h \
	./plugin/pluginloader.h \
	./plugin/pluginmanager.h \
	./plugin/pluginpreferencesutils.h \
	./plugin/streamdata.h \
	./plugin/pluginsutils.h

libplugin_la_SOURCES = \
	./plugin/jamipluginmanager.cpp \
	./plugin/pluginloader.cpp \
	./plugin/pluginmanager.cpp \
	./plugin/pluginpreferencesutils.cpp \
	./plugin/pluginsutils.cpp \
	./plugin/chatservicesmanager.cpp \
	./plugin/webviewservicesmanager.cpp \
	./plugin/callservicesmanager.cpp \
	./plugin/preferenceservicesmanager.cpp

libjami_la_LIBADD += libplugin.la

endif
