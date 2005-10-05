if NEWGUI_CODE
officialdir = official
endif
if MAINTENER_CODE
serverdir = server
serverlib = server/libsflphoneguiserver.la
else
serverdir = qt
serverlib = qt/libsflphoneqt.la
endif


SUBDIRS = $(serverdir)

noinst_LTLIBRARIES = libguiframework.la

libguiframework_la_SOURCES = guiframework.cpp guiframework.h

libguiframework_la_CPPFLAGS = -I$(top_srcdir)
libguiframework_la_CXXFLAGS = $(libccext2_CFLAGS)
libguiframework_la_LIBADD = $(serverlib)
