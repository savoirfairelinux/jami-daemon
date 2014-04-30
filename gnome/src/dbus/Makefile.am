include ../../globals.mak

noinst_LTLIBRARIES=libdbus.la

BUILT_SOURCES=                      \
    callmanager-glue.h              \
    configurationmanager-glue.h     \
    instance-glue.h                 \
    marshaller.c                    \
    marshaller.h

if SFL_PRESENCE
BUILT_SOURCES+=presencemanager-glue.h
presencemanager-glue.h: presencemanager-introspec.xml Makefile.am
	dbus-binding-tool --mode=glib-client $< > $@
endif

if SFL_VIDEO
BUILT_SOURCES+=videomanager-glue.h
videomanager-glue.h: videomanager-introspec.xml Makefile.am
	dbus-binding-tool --mode=glib-client $< > $@
endif

# Rule to generate the binding headers
%-glue.h: %-introspec.xml Makefile.am
	dbus-binding-tool --mode=glib-client $< > $@

# Create the necessary marshallers for the signals implemented by the library
marshaller.c: marshaller.list
	glib-genmarshal --body --g-fatal-warnings $< > $@

marshaller.h: marshaller.list
	glib-genmarshal --header --g-fatal-warnings $< > $@

libdbus_la_SOURCES=	\
	dbus.c		\
	dbus.h		\
    $(BUILT_SOURCES)

libdbus_la_LDFLAGS= $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) \
					$(GTK_LDFLAGS) $(GLIB_LDFLAGS) \
					$(GCONF_LDFLAGS)

libdbus_la_CFLAGS= $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
				   $(GTK_CFLAGS) $(GLIB_CFLAGS) \
				   $(GCONF_CFLAGS)

EXTRA_DIST= marshaller.list
CLEANFILES= $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
