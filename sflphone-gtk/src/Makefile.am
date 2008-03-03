bin_PROGRAMS = sflphone-gtk

sflphone_gtk_SOURCES = \
  main.c \
  dbus.c \
  mainwindow.c \
  calllist.c \
  dialpad.c \
  menus.c \
  calltree.c \
  screen.c \
  actions.c \
  configwindow.c \
  accountlist.c \
  accountwindow.c \
  marshaller.c \
  sliders.c \
  statusicon.c \
  codeclist.c \
  glwidget.c \
  MemManager.c
  
noinst_HEADERS =  actions.h dbus.h mainwindow.h calllist.h dialpad.h codeclist.h\
                  callmanager-glue.h configurationmanager-glue.h instance-glue.h menus.h calltree.h screen.h configwindow.h \
                  accountlist.h accountwindow.h marshaller.h sliders.h statusicon.h
EXTRA_DIST   = marshaller.list  
sflphone_gtk_LDADD = $(DEPS_LIBS)

AM_CPPFLAGS = $(DEPS_CFLAGS) \
              -DICONS_DIR=\""$(datadir)/sflphone"\" \
              -DICON_DIR=\""$(datadir)/pixmaps"\"
