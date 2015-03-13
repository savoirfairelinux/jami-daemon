include ../globals.mak

SUBDIRS = config contacts dbus widget icons video messaging

bin_PROGRAMS = ring-client-gnome

RING_LIBS=./contacts/libcontacts.la ./config/libconfig.la \
              ./dbus/libdbus.la ./widget/libwidget.la ./icons/libicons.la \
              ./messaging/libmessaging.la

DL_LIBS = -ldl
RT_LIBS = -lrt

if SFL_VIDEO
RING_LIBS+=./video/libvideo.la
endif

noinst_LTLIBRARIES = libring_client.la

libring_client_la_SOURCES = \
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
  ring_client.h \
  ring_client.c \
  ring_options.c \
  ring_options.h

libring_client_la_SOURCES+=presence.c presence.h presencewindow.c presencewindow.h

libring_client_la_LIBADD = @CLUTTER_LIBS@ @CLUTTERGTK_LIBS@ $(RING_LIBS)
libring_client_la_LDFLAGS = @CLUTTER_LDFLAGS@ @DBUSGLIB_LIBS@ @LIBNOTIFY_LIBS@ \
                                @X11_LIBS@ @GTK_LIBS@ @GLIB_LIBS@ \
                                $(DL_LIBS) $(RT_LIBS) \
                                @CLUTTERGTK_LDFLAGS@

libring_client_la_CFLAGS = @CLUTTER_CFLAGS@ @DBUSGLIB_CFLAGS@ @LIBNOTIFY_CFLAGS@ \
                               @GTK_CFLAGS@ \
                               @CLUTTERGTK_CFLAGS@


ring_client_gnome_SOURCES = main.c
ring_client_gnome_CFLAGS = @DBUSGLIB_CFLAGS@ @LIBNOTIFY_CFLAGS@ \
                               @GTK_CFLAGS@

ring_client_gnome_LDADD = libring_client.la

# add symbolic link
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(DESTDIR)$(bindir); ln -sf ring-client-gnome ring

uninstall-local:
	-test -h $(DESTDIR)$(bindir)/ring && rm $(DESTDIR)$(bindir)/ring

-include $(top_srcdir)/git.mk
