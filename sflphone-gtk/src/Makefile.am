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
  sliders.c
  
noinst_HEADERS =  actions.h dbus.h mainwindow.h calllist.h dialpad.h \
                  callmanager-glue.h menus.h calltree.h screen.h configwindow.h \
                  accountlist.h accountwindow.h marshaller.h sliders.h
EXTRA_DIST   = marshaller.list
sflphone_gtk_LDADD = $(DEPS_LIBS)

AM_CPPFLAGS = $(DEPS_CFLAGS) \
              -DPIXMAPS_DIR=\""$(datadir)/pixmaps"\"
