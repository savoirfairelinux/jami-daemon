include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient_rest.la

libclient_rest_la_SOURCES = \
    restclient.cpp \
    restclient.h \
    restconfigurationmanager.cpp \
    restconfigurationmanager.h \
    restvideomanager.cpp \
    restvideomanager.h

libclient_rest_la_CXXFLAGS = \
    -std=c++14 \
    -g \
    -Wall \
    -Wextra \
    -Wno-reorder \
    -Wno-unused-variable \
    -Wno-unused-parameter \
    -pedantic \
    -I$(top_srcdir)/src

libclient_rest_la_LDFLAGS = \
    -lpthread \
    -lrestbed
