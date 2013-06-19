ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
SUBDIRS = src pixmaps tests man po doc data

CFLAGS=-Wall -Werror -Wextra

uidir=$(datadir)/sflphone/ui
ui_DATA=src/ui.xml

dist_noinst_DATA = $(gsettings_in_file)

EXTRA_DIST  = sflphone.desktop.in m4 $(ui_DATA) $(apps_DATA)
appsdir = $(datadir)/applications
apps_in_files = sflphone.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)

DISTCLEANFILES= sflphone.desktop

MAINTAINERCLEANFILES = \
						$(srcdir)/INSTALL \
						$(srcdir)/aclocal.m4 \
						$(srcdir)/autoscan.log \
						$(srcdir)/compile \
						$(srcdir)/config.guess \
						$(srcdir)/config.h.in \
						$(srcdir)/config.sub \
						$(srcdir)/configure.scan \
						$(srcdir)/depcomp \
						$(srcdir)/install-sh \
						$(srcdir)/ltmain.sh \
						$(srcdir)/missing \
						$(srcdir)/mkinstalldirs \
						`find "$(srcdir)" -type f -name Makefile.in -print` \
						build-aux/* \
						m4/* \
						ABOUT-NLS \
						data/gschemas.compiled \
						po/Makevars.template \
						po/Rules-quot \
						po/boldquot.sed \
						po/en@boldquot.header \
						po/en@quot.header \
						po/insert-header.sin \
						po/quot.sed \
						po/remove-potcdate.sin

-include $(top_srcdir)/git.mk
