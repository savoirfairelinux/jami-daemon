ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
SUBDIRS = src pixmaps tests man po doc data

CFLAGS=-Wall -Werror -Wextra

uidir=$(datadir)/sflphone/ui
ui_DATA=src/ui.xml

dist_noinst_DATA = $(gsettings_in_file)

EXTRA_DIST  = sflphone.desktop.in gnome-doc-utils.make m4 $(ui_DATA) $(apps_DATA)
appsdir = $(datadir)/applications
apps_in_files = sflphone.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)

DISTCLEANFILES= sflphone.desktop gnome-doc-utils.make
