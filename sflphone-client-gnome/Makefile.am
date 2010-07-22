SUBDIRS = src pixmaps tests man po doc

ACLOCAL_AMFLAGS = -I m4

GCONFTOOL=gconftool-2

GNOME_DOC=gnome-doc-utils.make

uidir=$(datadir)/sflphone/ui
ui_DATA=src/ui.xml

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = sflphone-client-gnome.schemas

EXTRA_DIST   = sflphone.desktop.in $(GNOME_DOC) m4 $(UI_DATA) $(schema_DATA)
appsdir = $(datadir)/applications
apps_in_files = sflphone.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)
#@INTLTOOL_DESKTOP_RULE@

DISTCLEANFILES= sflphone.desktop $(GNOME_DOC) 

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schema_DATA)
