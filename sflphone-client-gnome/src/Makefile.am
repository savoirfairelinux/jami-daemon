include ../globals.mak

bin_PROGRAMS = sflphone-client-gnome

SUBDIRS = config contacts dbus
  
NOFIFY_LIBS = -lnotify
LOG4C = -llog4c

SFLPHONEGTK_LIBS=./contacts/libcontacts.la ./config/libconfig.la ./dbus/libdbus.la

sflphone_client_gnome_SOURCES = \
  main.c \
  errors.c \
  sflnotify.c \
  assistant.c \
  mainwindow.c \
  dialpad.c \
  menus.c \
  toolbar.c \
  callable_obj.c \
  actions.c \
  accountlist.c \
  sliders.c \
  statusicon.c \
  codeclist.c \
  timestamp.c \
  reqaccount.c

noinst_HEADERS =  actions.h sflnotify.h mainwindow.h dialpad.h codeclist.h \
                  assistant.h timestamp.h reqaccount.h errors.h sflphone_const.h \
                  menus.h accountlist.h sliders.h statusicon.h callable_obj.h toolbar.h


sflphone_client_gnome_LDADD = $(DEPS_LIBS) $(NOTIFY_LIBS) $(SFLPHONEGTK_LIBS) $(LIBSEXY_LIBS) $(LOG4C)

# add symbolic link	     
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(DESTDIR)$(bindir); ln -sf sflphone-client-gnome sflphone 

uninstall-local:
	-test -h $(DESTDIR)$(bindir)/sflphone && rm $(DESTDIR)$(bindir)/sflphone
