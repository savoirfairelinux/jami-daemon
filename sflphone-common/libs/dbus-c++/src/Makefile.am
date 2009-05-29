AM_CPPFLAGS = \
	$(dbus_CFLAGS) \
	$(glib_CFLAGS) \
	$(ecore_CFLAGS) \
	$(PRIVATE_CFLAGS) \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include

if ENABLE_GLIB
GLIB_H = $(HEADER_DIR)/glib-integration.h
GLIB_CPP = glib-integration.cpp
endif

if ENABLE_ECORE
ECORE_H = $(HEADER_DIR)/ecore-integration.h
ECORE_CPP = ecore-integration.cpp
endif

HEADER_DIR  = $(top_srcdir)/include/dbus-c++
HEADER_FILES = \
	$(HEADER_DIR)/dbus.h \
	$(HEADER_DIR)/types.h \
	$(HEADER_DIR)/connection.h \
	$(HEADER_DIR)/property.h \
	$(HEADER_DIR)/debug.h \
	$(HEADER_DIR)/error.h \
	$(HEADER_DIR)/interface.h \
	$(HEADER_DIR)/message.h \
	$(HEADER_DIR)/dispatcher.h \
	$(HEADER_DIR)/object.h \
	$(HEADER_DIR)/pendingcall.h \
	$(HEADER_DIR)/server.h \
	$(HEADER_DIR)/debug.h \
	$(HEADER_DIR)/util.h \
	$(HEADER_DIR)/refptr_impl.h \
	$(HEADER_DIR)/introspection.h \
	$(HEADER_DIR)/api.h \
	$(HEADER_DIR)/eventloop.h \
	$(HEADER_DIR)/eventloop-integration.h \
	$(GLIB_H) $(ECORE_H)

lib_includedir=$(includedir)/dbus-c++-1/dbus-c++/
lib_include_HEADERS = $(HEADER_FILES)

lib_LTLIBRARIES = libdbus-c++-1.la
libdbus_c___1_la_SOURCES = $(HEADER_FILES) interface.cpp object.cpp introspection.cpp debug.cpp types.cpp connection.cpp connection_p.h property.cpp dispatcher.cpp dispatcher_p.h pendingcall.cpp pendingcall_p.h error.cpp internalerror.h message.cpp message_p.h server.cpp server_p.h eventloop.cpp eventloop-integration.cpp $(GLIB_CPP) $(ECORE_CPP)
libdbus_c___1_la_LIBADD = $(dbus_LIBS) $(glib_LIBS) $(pthread_LIBS) $(ecore_LIBS)

MAINTAINERCLEANFILES = \
	Makefile.in
