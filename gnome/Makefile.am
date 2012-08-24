SUBDIRS = src pixmaps tests man po doc

CFLAGS=-Wall -Werror -Wextra

ACLOCAL_AMFLAGS = -I m4

GNOME_DOC=gnome-doc-utils.make

uidir=$(datadir)/sflphone/ui
ui_DATA=src/ui.xml

@INTLTOOL_XML_NOMERGE_RULE@
gsettings_in_file = data/org.sflphone.SFLphone.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_file:.xml.in=.xml)
@GSETTINGS_RULES@

dist_noinst_DATA = $(gsettings_in_file)

EXTRA_DIST  = sflphone.desktop.in $(GNOME_DOC) m4 $(ui_DATA) $(apps_DATA)
appsdir = $(datadir)/applications
apps_in_files = sflphone.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)

CLEANFILES = $(gsettings_SCHEMAS)

MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)

DISTCLEANFILES= sflphone.desktop $(GNOME_DOC)
