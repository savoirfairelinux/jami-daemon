include ../globals.mak

TESTS = check_global check_contacts check_config check_dbus
check_PROGRAMS = check_global check_contacts check_config check_dbus

SFLPHONE_LIBS = $(top_builddir)/src/contacts/libcontacts.la \
				$(top_builddir)/src/dbus/libdbus.la \
				$(top_builddir)/src/config/libconfig.la

check_global_SOURCES = check_global.c $(top_srcdir)/src/accountlist.c $(top_srcdir)/src/logger.c $(top_srcdir)/src/str_utils.c
check_global_CFLAGS = @CHECK_CFLAGS@ @GTK_CFLAGS@ @GLIB_CFLAGS@ @DBUSGLIB_CFLAGS@ @GCONF_CFLAGS@
check_global_LDADD = $(SFLPHONE_LIBS) @CHECK_LIBS@ @GLIB_LIBS@ @GTK_LIBS@ @DBUSGLIB_LIBS@ @GCONF_LIBS@

check_contacts_SOURCES = check_contacts.c
check_contacts_CFLAGS = @CHECK_CFLAGS@ @GTK_CFLAGS@
check_contacts_LDADD = $(SFLPHONE_LIBS) @CHECK_LIBS@

check_config_SOURCES = check_config.c
check_config_CFLAGS = @CHECK_CFLAGS@ @GTK_CFLAGS@
check_config_LDADD = $(SFLPHONE_LIBS) @CHECK_LIBS@

check_dbus_SOURCES = check_dbus.c $(top_srcdir)/src/logger.c \
					 $(top_srcdir)/src/accountlist.c \
					 $(top_srcdir)/src/actions.c \
					 $(top_srcdir)/src/codeclist.c \
					 $(top_srcdir)/src/dialpad.c \
					 $(top_srcdir)/src/mainwindow.c \
					 $(top_srcdir)/src/reqaccount.c \
					 $(top_srcdir)/src/sflnotify.c \
					 $(top_srcdir)/src/sliders.c \
					 $(top_srcdir)/src/statusicon.c \
					 $(top_srcdir)/src/callable_obj.c \
					 $(top_srcdir)/src/eel-gconf-extensions.c \
					 $(top_srcdir)/src/uimanager.c \
					 $(top_srcdir)/src/conference_obj.c \
					 $(top_srcdir)/src/shortcuts.c \
					 $(top_srcdir)/src/imwindow.c \
					 $(top_srcdir)/src/str_utils.c


if SFL_VIDEO
check_dbus_SOURCES+=$(top_srcdir)/src/video/video_renderer.c
endif

check_dbus_CFLAGS = @CHECK_CFLAGS@ @GTK_CFLAGS@ @DBUSGLIB_CFLAGS@ @GCONF_CFLAGS@ @WEBKIT_CFLAGS@
if SFL_VIDEO
check_dbus_CFLAGS+=@CLUTTER_CFLAGS@ @CLUTTERGTK_CFLAGS@
endif

check_dbus_LDADD = $(SFLPHONE_LIBS) @CHECK_LIBS@ @GCONF_LIBS@ \
				   $(top_builddir)/src/widget/libwidget.la \
				   $(top_builddir)/src/icons/libicons.la @X11_LIBS@ \
				   @JAVASCRIPT_CORE_GTK_LIBS@ -ldl
