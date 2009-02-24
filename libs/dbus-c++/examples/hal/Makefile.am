AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include

noinst_PROGRAMS = hal-listen

hal_listen_SOURCES = hal-listen.h hal-listen.cpp
hal_listen_LDADD = $(top_builddir)/src/libdbus-c++-1.la

MAINTAINERCLEANFILES = \
	Makefile.in
