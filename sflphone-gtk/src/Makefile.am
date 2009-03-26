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

noinst_HEADERS =  actions.h dbus.h sflnotify.h mainwindow.h dialpad.h codeclist.h assistant.h timestamp.h reqaccount.h \
                  callmanager-glue.h errors.h sflphone_const.h configurationmanager-glue.h instance-glue.h menus.h \
                  accountlist.h marshaller.h sliders.h statusicon.h call.h toolbar.h

EXTRA_DIST   = marshaller.list  

sflphone_gtk_LDADD = $(DEPS_LIBS) $(NOTIFY_LIBS) $(SEXY_LIBS) $(SFLPHONEGTK_LIBS)

# add symbolic link	     
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(bindir); ln -sf sflphone-gtk sflphone 

uninstall-local:
	-test -h $(bindir)/sflphone && rm $(bindir)/sflphone
