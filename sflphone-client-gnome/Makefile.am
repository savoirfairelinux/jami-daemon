SUBDIRS = src pixmaps tests man po doc

ACLOCAL_AMFLAGS = -I m4

GNOME_DOC=gnome-doc-utils.make

log4crc_DATA = log4crc
log4crcdir = $(datadir)/sflphone

uidir=$(DESTDIR)$(datadir)/sflphone/ui
ui_DATA=src/ui.xml

EXTRA_DIST   = $(log4crc_DATA) sflphone.desktop.in $(GNOME_DOC) m4 $(UI_DATA)
appsdir = $(datadir)/applications
apps_in_files = sflphone.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)
#@INTLTOOL_DESKTOP_RULE@

DISTCLEANFILES= sflphone.desktop $(GNOME_DOC)
