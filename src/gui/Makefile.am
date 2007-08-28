serverdir = server 
serverlib = server/libsflphoneguiserver.la

if WITH_QT
qtdir = qt
else
qtdir = 
endif

#libexec_PROGRAMS = qt/sflphone-qt

#qt/sflphone-qt:
#	cd qt && make


SUBDIRS = $(serverdir) $(qtdir) cli

noinst_LTLIBRARIES = libguiframework.la

libguiframework_la_SOURCES = guiframework.cpp guiframework.h

libguiframework_la_CPPFLAGS = -I$(top_srcdir)/libs -I$(top_srcdir)/src
libguiframework_la_CXXFLAGS = $(libccext2_CFLAGS) $(libdbuscpp_CFLAGS)
libguiframework_la_LIBADD = $(serverlib)
