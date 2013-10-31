include ../globals.mak

SUBDIRS = config contacts dbus widget icons video messaging

bin_PROGRAMS = sflphone-client-gnome

SFLPHONE_LIBS=./contacts/libcontacts.la ./config/libconfig.la \
              ./dbus/libdbus.la ./widget/libwidget.la ./icons/libicons.la \
              ./messaging/libmessaging.la

DL_LIBS = -ldl
RT_LIBS = -lrt

if SFL_VIDEO
SFLPHONE_LIBS+=./video/libvideo.la
endif

noinst_LTLIBRARIES = libsflphone_client.la

libsflphone_client_la_SOURCES = \
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
  shortcuts.c \
  str_utils.c \
  seekslider.c \
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
  str_utils.h \
  seekslider.h \
  account_schema.h \
  history_loader.c \
  history_loader.h \
  sflphone_client.h \
  sflphone_client.c \
  sflphone_options.c \
  sflphone_options.h

if SFL_PRESENCE
libsflphone_client_la_SOURCES+=presence.c presence.h presencewindow.c presencewindow.h
endif

libsflphone_client_la_LIBADD = @CLUTTER_LIBS@ @CLUTTERGTK_LIBS@ $(SFLPHONE_LIBS)
libsflphone_client_la_LDFLAGS = @CLUTTER_LDFLAGS@ @DBUSGLIB_LIBS@ @LIBNOTIFY_LIBS@ \
                                @X11_LIBS@ @GTK_LIBS@ @GLIB_LIBS@ \
                                $(DL_LIBS) $(RT_LIBS) \
                                @CLUTTERGTK_LDFLAGS@

libsflphone_client_la_CFLAGS = @CLUTTER_CFLAGS@ @DBUSGLIB_CFLAGS@ @LIBNOTIFY_CFLAGS@ \
                               @GTK_CFLAGS@ \
                               @CLUTTERGTK_CFLAGS@


sflphone_client_gnome_SOURCES = main.c
sflphone_client_gnome_CFLAGS = @DBUSGLIB_CFLAGS@ @LIBNOTIFY_CFLAGS@ \
                               @GTK_CFLAGS@

sflphone_client_gnome_LDADD = libsflphone_client.la

# add symbolic link
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(DESTDIR)$(bindir); ln -sf sflphone-client-gnome sflphone

uninstall-local:
	-test -h $(DESTDIR)$(bindir)/sflphone && rm $(DESTDIR)$(bindir)/sflphone

-include $(top_srcdir)/git.mk
