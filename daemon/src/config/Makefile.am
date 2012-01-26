noinst_LTLIBRARIES = libconfig.la

libconfig_la_SOURCES = \
	sfl_config.cpp \
	yamlemitter.cpp \
	yamlparser.cpp \
	yamlnode.cpp

noinst_HEADERS = \
	sfl_config.h \
	serializable.h \
	yamlemitter.h \
	yamlparser.h \
	yamlnode.h

libconfig_la_LDFLAGS = @yaml_LIBS@

libconfig_la_CXXFLAGS = @yaml_CFLAGS@ -I $(top_srcdir)/src
