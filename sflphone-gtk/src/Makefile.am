bin_PROGRAMS = sflphone-gtk

sflphone_gtk_SOURCES = \
  main.c \
  errors.c \
  dbus.c \
  sflnotify.c \
  assistant.c \
  mainwindow.c \
  calllist.c \
  dialpad.c \
  menus.c \
  calltab.c \
  calltree.c \
  historyfilter.c \
  actions.c \
  configwindow.c \
  accountlist.c \
  accountwindow.c \
  marshaller.c \
  sliders.c \
  statusicon.c \
  codeclist.c \
  MemManager.c
  
noinst_HEADERS =  actions.h dbus.h sflnotify.h mainwindow.h calllist.h dialpad.h codeclist.h assistant.h\
                  callmanager-glue.h errors.h sflphone_const.h configurationmanager-glue.h instance-glue.h menus.h calltab.h calltree.h configwindow.h \
                  historyfilter.h accountlist.h accountwindow.h marshaller.h sliders.h statusicon.h
EXTRA_DIST   = marshaller.list  
sflphone_gtk_LDADD = $(DEPS_LIBS) -lnotify

AM_CPPFLAGS = $(DEPS_CFLAGS) \
              -DICONS_DIR=\""$(datadir)/sflphone"\" \
	      -DCODECS_DIR=\""$(libdir)/sflphone/codecs"\" 

# add symbolic link	     
install-exec-local:
	ln -sf $(bindir)/sflphone-gtk $(bindir)/sflphone 

uninstall-local:
	-test -h $(bindir)/sflphone && rm $(bindir)/sflphone
