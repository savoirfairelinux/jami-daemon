include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient_rest.la

libclient_rest_la_SOURCES = \
	connection.h \
	http_status.h \
	json.h \
	manager.h \
	parserHTTP.cpp \
	parserHTTP.h \
	response.cpp \
	response.h \
	router.cpp \
	router.h \
	server.h \
	restclient.cpp \
	restclient.h

libclient_rest_la_CXXFLAGS = \
	-std=c++14 \
	-g \
	-Wall \
	-Wextra \
	-Wno-reorder \
	-pedantic \
	-I../ \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/dring

libclient_rest_la_LDFLAGS = \
	-lpthread \
	-lboost_system \
	-lcrypto \
	-lssl
