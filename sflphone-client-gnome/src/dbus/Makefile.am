include ../../globals.mak

noinst_LTLIBRARIES=libdbus.la

BUILD_SOURCES=                      \
    callmanager-glue.h              \
    configurationmanager-glue.h     \
    instance-glue.h                 \
    marshaller.c                    \
    marshaller.h

# Rule to generate the binding headers
%-glue.h: ../../../src/dbus/%-introspec.xml Makefile.am
	dbus-binding-tool --mode=glib-client $<  > $@

# Create the necessary marshallers for the signals implemented by the library
marshaller.c: marshaller.list
	glib-genmarshal --body --g-fatal-warnings $< > $@

marshaller.h: marshaller.list
	glib-genmarshal --header --g-fatal-warnings $< > $@

libdbus_la_SOURCES=     \
    dbus.c              \
    $(BUILD_SOURCES)

libdbus_la_LDFLAGS= @DEPS_LDFLAGS@
libdbus_la_CFLAGS= @DEPS_CFLAGS@

EXTRA_DIST= \
    marshaller.list

CLEANFILES= \
    $(BUILD_SOURCES)
