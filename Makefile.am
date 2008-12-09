# Makefile.am

unittest:
	@(cd test; make)
	@echo ""
	@echo "NOTICE: Unitary tests successfully build"
	@echo "Go in the test directory to run them"
	@echo ""

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libs src ringtones po man 
EXTRA_DIST = m4/*.m4 tools/*.sh platform/* images/* README.gentoo

