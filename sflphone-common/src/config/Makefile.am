SUBDIRS = 

noinst_LTLIBRARIES = libconfig.la

libconfig_la_SOURCES = \
	config.cpp \
	yamlengine.cpp \
	yamlemitter.cpp \
	yamlparser.cpp \
	yamlnode.cpp

noinst_HEADERS = \
	config.h \
	engine.h \
	serializable.h \
	yamlengine.h \
	yamlemitter.h \
	yamlparser.h \
	yamlnode.h

libconfig_la_LDFLAGS = @yaml_LIBS@

libconfig_la_CFLAGS = @yaml_CFLAGS@