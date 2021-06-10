include $(top_srcdir)/globals.mk

AM_CXXFLAGS += -I$(top_srcdir)/src -I. -include common.h
AM_LDFLAGS += $(top_builddir)/src/libring.la
check_PROGRAMS =

lib_LTLIBRARIES = libfuzz.la

libfuzz_la_SOURCES = lib/utils.cpp
