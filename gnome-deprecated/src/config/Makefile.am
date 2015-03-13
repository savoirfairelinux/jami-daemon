include ../../globals.mak

noinst_LTLIBRARIES = libconfig.la

libconfig_la_SOURCES = \
  addressbook-config.c \
  shortcuts-config.c \
  assistant.c \
  preferencesdialog.c \
  accountlistconfigdialog.c \
  zrtpadvanceddialog.c \
  tlsadvanceddialog.c \
  accountconfigdialog.c \
  hooks-config.c \
  audioconf.c \
  utils.c \
  accountconfigdialog.h \
  accountlistconfigdialog.h \
  addressbook-config.h \
  assistant.h \
  audioconf.h \
  hooks-config.h \
  preferencesdialog.h \
  shortcuts-config.h \
  tlsadvanceddialog.h \
  utils.h \
  zrtpadvanceddialog.h

if SFL_VIDEO
libconfig_la_SOURCES+=videoconf.c videoconf.h
endif

if SFL_VIDEO
SFL_VIDEO_LDFLAGS=@CLUTTER_LIBS@ @CLUTTERGTK_LIBS@
endif

libconfig_la_LDFLAGS = $(DBUSGLIB_LDFLAGS) $(LIBNOTIFY_LDFLAGS) \
					   $(GTK_LDFLAGS) $(GLIB_LDFLAGS) \
					   $(SFL_VIDEO_LDFLAGS) 

if SFL_VIDEO
SFL_VIDEO_LIBADD=@CLUTTER_LIBS@ @CLUTTERGTK_LIBS@
endif

libconfig_la_LIBADD = $(DBUSGLIB_LIBS) $(LIBNOTIFY_LIBS) \
					  $(GTK_LIBS) $(GLIB_LIBS) $(SFL_VIDEO_LIBADD)

if SFL_VIDEO
SFL_VIDEO_CFLAGS=@CLUTTER_CFLAGS@ @CLUTTERGTK_CFLAGS@
endif

libconfig_la_CFLAGS = $(DBUSGLIB_CFLAGS) $(LIBNOTIFY_CFLAGS) \
					  $(GTK_CFLAGS) $(GLIB_CFLAGS) \
					  $(SFL_VIDEO_CFLAGS)

-include $(top_srcdir)/git.mk
