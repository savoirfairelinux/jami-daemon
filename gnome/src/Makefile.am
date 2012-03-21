include ../globals.mak

bin_PROGRAMS = sflphone-client-gnome

SUBDIRS = config contacts dbus widget icons
if SFL_VIDEO
SUBDIRS += video
endif

NOFIFY_LIBS = -lnotify
LD_LIBS = -ldl

SFLPHONEGTK_LIBS=./contacts/libcontacts.la ./config/libconfig.la \
                 ./dbus/libdbus.la ./widget/libwidget.la ./icons/libicons.la

if SFL_VIDEO
SFLPHONEGTK_LIBS+=./video/libvideo.la
endif


sflphone_client_gnome_SOURCES = \
  main.c \
  logger.c \
  uimanager.c \
  sflnotify.c \
  mainwindow.c \
  imwindow.c \
  dialpad.c \
  callable_obj.c \
  conference_obj.c \
  actions.c \
  accountlist.c \
  sliders.c \
  statusicon.c \
  codeclist.c \
  reqaccount.c \
  eel-gconf-extensions.c \
  shortcuts.c \
  str_utils.c

noinst_HEADERS =  actions.h sflnotify.h mainwindow.h dialpad.h codeclist.h \
                  reqaccount.h sflphone_const.h uimanager.h accountlist.h \
				  sliders.h statusicon.h callable_obj.h conference_obj.h \
                  shortcuts.h eel-gconf-extensions.h logger.h imwindow.h \
				  unused.h str_utils.h

sflphone_client_gnome_LDADD = $(DBUSGLIB_LIBS) $(LIBNOTIFY_LIBS) \
                              $(NOTIFY_LIBS) $(SFLPHONEGTK_LIBS) $(X11_LIBS) \
                              $(GTK_LIBS) $(GLIB_LIBS) $(WEBKIT_LIBS) \
                              $(LD_LIBS) $(GCONF_LIBS) \
                              $(JAVASCRIPT_CORE_GTK_LIBS)

sflphone_client_gnome_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
                               $(NOTIFY_CFLAGS) $(GTK_CFLAGS) $(X11_CFLAGS) \
                               $(GLIB_CFLAGS) $(WEBKIT_CFLAGS) \
                               $(GCONF_CFLAGS) $(JAVASCRIPT_CORE_GTK_CFLAGS)

# add symbolic link
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(DESTDIR)$(bindir); ln -sf sflphone-client-gnome sflphone

uninstall-local:
	-test -h $(DESTDIR)$(bindir)/sflphone && rm $(DESTDIR)$(bindir)/sflphone

# all: indent
