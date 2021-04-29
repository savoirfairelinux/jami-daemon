include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libplugin.la

noinst_HEADERS = \
	callservicesmanager.h \
	chathandler.h \
	chatservicesmanager.h \
	jamiplugin.h \
	jamipluginmanager.h \
	mediahandler.h \
	pluginloader.h \
	pluginmanager.h \
	pluginpreferencesutils.h \
	streamdata.h \
	pluginsutils.h

libplugin_la_SOURCES = \
	jamipluginmanager.cpp \
	pluginloader.cpp \
	pluginmanager.cpp \
	pluginpreferencesutils.cpp \
	pluginsutils.cpp \
	chatservicesmanager.cpp \
	callservicesmanager.cpp

if HAVE_OSX
	libplugin_la_LIBADD = @MINIZIP_LIBS@
else
	libplugin_la_LIBADD = @ARCHIVE_LIBS@
endif

libplugin_la_CXXFLAGS = \
	-I./ \
	-I../ \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/ring\" \
	$(AM_CXXFLAGS)
