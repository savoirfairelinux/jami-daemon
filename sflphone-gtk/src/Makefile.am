include ../globals.mak

bin_PROGRAMS = sflphone-gtk

SUBDIRS = config contacts dbus
  
NOFIFY_LIBS = -lnotify
LOG4C = -llog4c

SFLPHONEGTK_LIBS=./contacts/libcontacts.la ./config/libconfig.la ./dbus/libdbus.la

sflphone_gtk_SOURCES = \
  main.c \
  errors.c \
  sflnotify.c \
  assistant.c \
  mainwindow.c \
  dialpad.c \
  menus.c \
  toolbar.c \
  call.c \
  actions.c \
  accountlist.c \
  marshaller.c \
  sliders.c \
  statusicon.c \
  codeclist.c \
  timestamp.c \
  reqaccount.c

noinst_HEADERS =  actions.h sflnotify.h mainwindow.h dialpad.h codeclist.h \
                  assistant.h timestamp.h reqaccount.h errors.h sflphone_const.h \
                  menus.h accountlist.h marshaller.h sliders.h statusicon.h call.h toolbar.h

EXTRA_DIST   = marshaller.list  

sflphone_gtk_LDADD = $(DEPS_LIBS) $(NOTIFY_LIBS) $(SFLPHONEGTK_LIBS) $(LIBSEXY_LIBS) $(LOG4C)

# add symbolic link	     
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(bindir); ln -sf sflphone-gtk sflphone 

uninstall-local:
	-test -h $(bindir)/sflphone && rm $(bindir)/sflphone
