# Makefile.am

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libs src ringtones skins doc
EXTRA_DIST = m4/*.m4 tools/*.sh platform/* images/* README.gentoo


.PHONY: doc
doc:
	$(MAKE) -C doc doc

.PHONY: doxygen-trac
doxygen-trac:
	$(MAKE) -C doc/doxygen doxygen-trac