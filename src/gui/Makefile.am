SUBDIRS = qt server

noinst_LTLIBRARIES = libguiframework.la

libguiframework_la_SOURCES = \
	guiframework.cpp guiframework.h

libguiframework_la_CPPFLAGS = -I$(top_srcdir)
libguiframework_la_CXXFLAGS = $(libccext2_CFLAGS)
libguiframework_la_LIBADD = qt/libsflphoneqt.la server/libsflphoneguiserver.la
