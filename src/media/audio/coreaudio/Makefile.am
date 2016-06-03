include $(top_srcdir)/globals.mak

if HAVE_OSX
noinst_LTLIBRARIES = libcoreaudiolayer.la
endif

if HAVE_IOS
noinst_LTLIBRARIES = libcoreaudiolayer.la
endif

libcoreaudiolayer_la_SOURCES = corelayer.cpp corelayer.h audiodevice.cpp audiodevice.h
libcoreaudiolayer_la_CXXFLAGS = -I$(top_srcdir)/src
