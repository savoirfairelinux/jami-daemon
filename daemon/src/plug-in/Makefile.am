include ../../globals.mak

SUBDIRS=test

noinst_LTLIBRARIES = libplugin.la

libplugin_la_SOURCES = \
		pluginmanager.cpp \
		librarymanager.cpp \
		pluginmanager.h librarymanager.h plugin.h

libplugin_la_CXXFLAGS = -I $(top_srcdir)/src/plug-in
