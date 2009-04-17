include ../globals.mak

TESTS = check_global check_contacts check_config check_dbus
check_PROGRAMS = check_global check_contacts check_config check_dbus

###########################################################

check_global_SOURCES =		check_global.c
check_global_CFLAGS =		@CHECK_CFLAGS@ @DEPS_CFLAGS@
check_global_LDADD =		@CHECK_LIBS@

###########################################################

check_contacts_SOURCES =	check_contacts.c
							$(top_builddir)/src/eds.h
check_contacts_CFLAGS =		@CHECK_CFLAGS@ @DEPS_CFLAGS@
check_contacts_LDADD =		@CHECK_LIBS@

###########################################################

check_config_SOURCES =		check_config.c
check_config_CFLAGS = 		@CHECK_CFLAGS@ @DEPS_CFLAGS@
check_config_LDADD = 		@CHECK_LIBS@

###########################################################

check_dbus_SOURCES =	check_dbus.c
check_dbus_CFLAGS = 	@CHECK_CFLAGS@ @DEPS_CFLAGS@
check_dbus_LDADD = 		@CHECK_LIBS@

###########################################################
