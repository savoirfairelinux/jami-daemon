bin_PROGRAMS = sflphone-gtk

SUBDIRS=contactlist
  
# noinst_LTLIBRARIES = libsflphonegtk.la

NOFIFY_LIBS=-lnotify
SEXY_LIBS=-lsexy

#if GTK_CHECK_VERSION(2,10,0)  
STATUSICON_HEADER = statusicon.h
STATUSICON_SOURCE = statusicon.c
#elif
#STATUSICON_HEADER = 
#STATUSICON_SOURCE = 
#endif

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
  searchfilter.c \
  actions.c \
  configwindow.c \
  accountlist.c \
  accountwindow.c \
  marshaller.c \
  sliders.c \
  $(STATUSICON_SOURCE) \
  codeclist.c \
  timestamp.c \
  reqaccount.c \
  addressbook-config.c \
  hooks-config.c

noinst_HEADERS =  actions.h dbus.h sflnotify.h mainwindow.h calllist.h dialpad.h audioconf.h codeclist.h assistant.h \
                  callmanager-glue.h errors.h sflphone_const.h configurationmanager-glue.h instance-glue.h menus.h calltab.h calltree.h configwindow.h \
                  accountlist.h accountwindow.h marshaller.h sliders.h $(STATUSICON_HEADER) timestamp.h searchfilter.h reqaccount.h addressbook-config.h hooks-config.h

EXTRA_DIST   = marshaller.list  

sflphone_gtk_LDADD = $(DEPS_LIBS) $(NOTIFY_LIBS) $(SEXY_LIBS) \
			./contactlist/libcontact.la


AM_CPPFLAGS = $(DEPS_CFLAGS) \
              -DICONS_DIR=\""$(prefix)/share/sflphone"\" \
	      	-DCODECS_DIR=\""$(prefix)/lib/sflphone/codecs"\" 

# add symbolic link	     
install-exec-local:
	echo $(prefix); echo $(bindir); echo $(datadir); echo $(libdir)
	cd $(bindir); ln -sf sflphone-gtk sflphone 

uninstall-local:
	-test -h $(bindir)/sflphone && rm $(bindir)/sflphone
