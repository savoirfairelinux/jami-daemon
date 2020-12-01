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
	streamdata.h

libplugin_la_SOURCES = \
	jamipluginmanager.cpp \
	pluginloaderdl.cpp \
	pluginmanager.cpp 


libplugin_la_LIBADD = @ARCHIVE_LIBS@

libplugin_la_CXXFLAGS = \
	-I./ \
	-I../ \
	-DPREFIX=\"$(prefix)\" \
	-DPROGSHAREDIR=\"${datadir}/ring\"
