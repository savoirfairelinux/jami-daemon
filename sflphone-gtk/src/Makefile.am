include ../globals.mak

bin_PROGRAMS = sflphone-gtk

SUBDIRS = config contacts
  
NOFIFY_LIBS = -lnotify
SEXY_LIBS = -lsexy

SFLPHONEGTK_LIBS=./contacts/libcontacts.la ./config/libconfig.la

sflphone_gtk_SOURCES = \
  main.c \
  errors.c \
  dbus.c \
  sflnotify.c \
  assistant.c \
  mainwindow.c \
  calllist.c \
  dialpad.c \
  audioconf.c \
  menus.c \
  calltab.c \
  calltree.c \
  actions.c \
  configwindow.c \
  accountlist.c \
  accountwindow.c \
  marshaller.c \
  sliders.c \
  statusicon.c \
  codeclist.c \
  timestamp.c \
  reqaccount.c \
  hooks-config.c

noinst_HEADERS =  actions.h dbus.h sflnotify.h mainwindow.h calllist.h dialpad.h audioconf.h codeclist.h assistant.h \
                  callmanager-glue.h errors.h sflphone_const.h configurationmanager-glue.h instance-glue.h menus.h calltab.h calltree.h configwindow.h \
                  accountlist.h accountwindow.h marshaller.h sliders.h statusicon.h timestamp.h reqaccount.h hooks-config.h

EXTRA_DIST   = marshaller.list  

sflphone_gtk_LDADD = $(DEPS_LIBS) $(NOTIFY_LIBS) $(SEXY_LIBS) $(SFLPHONEGTK_LIBS)

# add symbolic link	     
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(bindir); ln -sf sflphone-gtk sflphone 

uninstall-local:
	-test -h $(bindir)/sflphone && rm $(bindir)/sflphone
