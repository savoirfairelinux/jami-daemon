# Makefile.am

if BUILD_TEST
TESTS_DIR=test
unittest:
	@(cd test; make)
	@echo ""
	@echo "NOTICE: Unitary tests successfully build"
	@echo "Go in the test directory to run them"
	@echo ""
else
unittest:
	@echo "SFLphone WARNING:"
	@echo " -- You need the cppunit devel package to compile the unitary tests."
endif

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libs src ringtones po man $(TESTS_DIR)
EXTRA_DIST = m4/*.m4 tools/*.sh platform/* images/* README.gentoo

