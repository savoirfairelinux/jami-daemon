include ../../globals.mak

GENHEADERS= \
	callmanager-glue.h \
	configurationmanager-glue.h \
	instance-glue.h

MARSHALLER_SOURCES= \
	marshaller.c \
	marshaller.h

EXTRA_DIST=marshaller.list  

CLEANFILES= \
	$(GENHEADERS) \
	$(MARSHALLER_SOURCES)

# Rule to generate the binding headers
%-glue.h: ../../../src/dbus/%-introspec.xml Makefile.am
	dbus-binding-tool --mode=glib-client $<  > $@

# Create the necessary marshallers for the signals implemented by the
# library

marshaller.h: marshaller.list Makefile.am
	glib-genmarshal --header --g-fatal-warnings $< > $@

marshaller.c: marshaller.list
	glib-genmarshal --body --g-fatal-warnings $< > $@

noinst_LTLIBRARIES = libdbus.la
libdbus_la_SOURCES = dbus.c \
	$(MARSHALLER_SOURCES) \
	$(GENHEADERS)

libdbus_la_LDFLAGS = @DEPS_LDFLAGS@
libdbus_la_CFLAGS = @DEPS_CFLAGS@ 
