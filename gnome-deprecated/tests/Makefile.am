include ../globals.mak

TESTS = check_global check_contacts check_config
TESTS_ENVIRONMENT = GSETTINGS_SCHEMA_DIR=../data
check_PROGRAMS = check_global check_contacts check_config

RING_LIBS= $(top_builddir)/src/libring_client.la \
               $(top_builddir)/src/contacts/libcontacts.la \
               $(top_builddir)/src/config/libconfig.la \
               $(top_builddir)/src/dbus/libdbus.la \
               $(top_builddir)/src/widget/libwidget.la \
               $(top_builddir)/src/icons/libicons.la

check_global_SOURCES = check_global.c

check_global_CFLAGS = @CHECK_CFLAGS@ @GTK_CFLAGS@ @GLIB_CFLAGS@ @DBUSGLIB_CFLAGS@
check_global_LDADD = $(RING_LIBS) @CHECK_LIBS@ @GLIB_LIBS@ @GTK_LIBS@ @DBUSGLIB_LIBS@ -ldl @X11_LIBS@

check_contacts_SOURCES = check_contacts.c
check_contacts_CFLAGS = @CHECK_CFLAGS@ @GTK_CFLAGS@
check_contacts_LDADD = $(RING_LIBS) @CHECK_LIBS@

check_config_SOURCES = check_config.c
check_config_CFLAGS = @CHECK_CFLAGS@ @GTK_CFLAGS@
check_config_LDADD = $(RING_LIBS) @CHECK_LIBS@

clean-local:
	rm -rf *.xml

-include $(top_srcdir)/git.mk
