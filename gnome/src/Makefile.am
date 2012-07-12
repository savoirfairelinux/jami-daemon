include ../globals.mak

SUBDIRS = config contacts dbus widget icons video messaging

bin_PROGRAMS = sflphone-client-gnome

SFLPHONE_LIBS=./contacts/libcontacts.la ./config/libconfig.la \
              ./dbus/libdbus.la ./widget/libwidget.la ./icons/libicons.la \
              ./messaging/libmessaging.la

DL_LIBS = -ldl

if SFL_VIDEO
SFLPHONE_LIBS+=./video/libvideo.la
endif

noinst_LTLIBRARIES = libsflphone_client.la

libsflphone_client_la_SOURCES = \
  logger.c \
  uimanager.c \
  sflnotify.c \
  mainwindow.c \
  dialpad.c \
  callable_obj.c \
  conference_obj.c \
  actions.c \
  accountlist.c \
  sliders.c \
  statusicon.c \
  codeclist.c \
  eel-gconf-extensions.c \
  shortcuts.c \
  str_utils.c \
  seekslider.c \
  gtk2_wrappers.c \
  actions.h \
  sflnotify.h \
  mainwindow.h \
  dialpad.h \
  codeclist.h \
  sflphone_const.h \
  uimanager.h \
  accountlist.h \
  sliders.h \
  statusicon.h \
  callable_obj.h \
  conference_obj.h \
  shortcuts.h \
  eel-gconf-extensions.h \
  logger.h \
  unused.h \
  str_utils.h \
  gtk2_wrappers.h \
  seekslider.h \
  account_schema.h

libsflphone_client_la_LIBADD = @CLUTTER_LIBS@ @CLUTTERGTK_LIBS@ $(SFLPHONE_LIBS)
libsflphone_client_la_LDFLAGS = @CLUTTER_LDFLAGS@ @DBUSGLIB_LIBS@ @LIBNOTIFY_LIBS@ \
                                @X11_LIBS@ @GTK_LIBS@ @GLIB_LIBS@ @WEBKIT_LIBS@ \
                                $(DL_LIBS) @GCONF_LIBS@ @JAVASCRIPT_CORE_GTK_LIBS@ \
                                @GTHREAD_LIBS@ @CLUTTERGTK_LDFLAGS@

libsflphone_client_la_CFLAGS = @CLUTTER_CFLAGS@ @DBUSGLIB_CFLAGS@ @LIBNOTIFY_CFLAGS@ \
                               @GTK_CFLAGS@ @WEBKIT_CFLAGS@ \
                               @GCONF_CFLAGS@ @JAVASCRIPT_CORE_GTK_CFLAGS@ \
                               @GTHREAD_CFLAGS@ @CLUTTERGTK_CFLAGS@


sflphone_client_gnome_SOURCES = main.c
sflphone_client_gnome_CFLAGS = @DBUSGLIB_CFLAGS@ @LIBNOTIFY_CFLAGS@ \
                               @GTK_CFLAGS@ @WEBKIT_CFLAGS@ \
                               @GCONF_CFLAGS@ @JAVASCRIPT_CORE_GTK_CFLAGS@ \
                               @GTHREAD_CFLAGS@
sflphone_client_gnome_LDADD = libsflphone_client.la

# add symbolic link
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(DESTDIR)$(bindir); ln -sf sflphone-client-gnome sflphone

uninstall-local:
	-test -h $(DESTDIR)$(bindir)/sflphone && rm $(DESTDIR)$(bindir)/sflphone
