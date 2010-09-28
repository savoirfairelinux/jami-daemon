include ../globals.mak

TESTS = check_global check_contacts check_config check_dbus
check_PROGRAMS = check_global check_contacts check_config check_dbus

SFLPHONE_LIBS = $(top_builddir)/src/contacts/libcontacts.la \
				$(top_builddir)/src/dbus/libdbus.la \
				$(top_builddir)/src/config/libconfig.la

SFLPHONE_OBJ = $(top_builddir)/src/accountlist.o \
				$(top_builddir)/src/actions.o \
				$(top_builddir)/src/assistant.o \
				$(top_builddir)/src/call.o \
				$(top_builddir)/src/codeclist.o \
				$(top_builddir)/src/dialpad.o \
				$(top_builddir)/src/errors.o \
				$(top_builddir)/src/mainwindow.o \
				$(top_builddir)/src/menus.o \
				$(top_builddir)/src/reqaccount.o \
				$(top_builddir)/src/sflnotify.o \
				$(top_builddir)/src/sliders.o \
				$(top_builddir)/src/statusicon.o \
				$(top_builddir)/src/timestamp.o \
				$(top_builddir)/src/toolbar.o

###########################################################

check_global_SOURCES =		check_global.c
check_global_CFLAGS =		@CHECK_CFLAGS@ @DEPS_CFLAGS@
check_global_LDADD =		$(SFLPHONE_LIBS) $(SFLPHONE_OBJ) @CHECK_LIBS@ $(DEPS_LIBS) @LIBSEXY_LIBS@

###########################################################

check_contacts_SOURCES =	check_contacts.c
check_contacts_CFLAGS =		@CHECK_CFLAGS@ @DEPS_CFLAGS@
check_contacts_LDADD =		$(SFLPHONE_LIBS) $(SFLPHONE_OBJ) @CHECK_LIBS@ $(DEPS_LIBS) @LIBSEXY_LIBS@

###########################################################

check_config_SOURCES =		check_config.c
check_config_CFLAGS = 		@CHECK_CFLAGS@ @DEPS_CFLAGS@
check_config_LDADD =		$(SFLPHONE_LIBS) $(SFLPHONE_OBJ) @CHECK_LIBS@ @DEPS_LIBS@ @LIBSEXY_LIBS@

###########################################################

check_dbus_SOURCES =		check_dbus.c
check_dbus_CFLAGS = 		@CHECK_CFLAGS@ @DEPS_CFLAGS@
check_dbus_LDADD =		$(SFLPHONE_LIBS) $(SFLPHONE_OBJ) @CHECK_LIBS@ @DEPS_LIBS@ @LIBSEXY_LIBS@

###########################################################
