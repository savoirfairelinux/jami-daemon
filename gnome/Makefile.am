SUBDIRS = src pixmaps webkit tests man po doc

CFLAGS=-Wall -Werror -Wextra

ACLOCAL_AMFLAGS = -I m4

GCONFTOOL=gconftool-2

GNOME_DOC=gnome-doc-utils.make

uidir=$(datadir)/sflphone/ui
ui_DATA=src/ui.xml

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = sflphone-client-gnome.schemas

EXTRA_DIST  = sflphone.desktop.in $(GNOME_DOC) m4 $(ui_DATA) $(schema_DATA) $(apps_DATA)
appsdir = $(datadir)/applications
apps_in_files = sflphone.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)

DISTCLEANFILES= sflphone.desktop $(GNOME_DOC) 

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_srcdir)/$(schema_DATA)
