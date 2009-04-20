SUBDIRS = src pixmaps tests

ACLOCAL_AMFLAGS = -I m4

log4crc_DATA = log4crc
log4crcdir = $(datadir)/sflphone

EXTRA_DIST   = $(log4crc_DATA) sflphone.desktop.in 
appsdir = $(datadir)/applications
apps_in_files = sflphone.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)
#@INTLTOOL_DESKTOP_RULE@

DISTCLEANFILES= sflphone.desktop
