include ../globals.mak

bin_PROGRAMS = sflphone-client-gnome

SUBDIRS = config contacts dbus widget icons
  
NOFIFY_LIBS = -lnotify
X11_LIBS = -lX11

SFLPHONEGTK_LIBS=./contacts/libcontacts.la ./config/libconfig.la ./dbus/libdbus.la ./widget/libwidget.la ./icons/libicons.la

sflphone_client_gnome_SOURCES = \
  main.c \
  errors.c \
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
  shortcuts.c  \
  eel-gconf-extensions.c

noinst_HEADERS =  actions.h sflnotify.h mainwindow.h dialpad.h codeclist.h \
                  reqaccount.h errors.h sflphone_const.h uimanager.h \
                  accountlist.h sliders.h statusicon.h callable_obj.h conference_obj.h \
                  shortcuts.h eel-gconf-extensions.h logger.h

sflphone_client_gnome_LDADD = $(DEPS_LIBS) $(NOTIFY_LIBS) $(SFLPHONEGTK_LIBS) $(LIBSEXY_LIBS) $(X11_LIBS) $(DBUSGLIB_LIBS)

# add symbolic link	     
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(DESTDIR)$(bindir); ln -sf sflphone-client-gnome sflphone 

uninstall-local:
	-test -h $(DESTDIR)$(bindir)/sflphone && rm $(DESTDIR)$(bindir)/sflphone

# all: indent
