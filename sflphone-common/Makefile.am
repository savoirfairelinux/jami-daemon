include globals.mak

# Makefile.am

SOURCES=

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

doc:
	@(cd doc; make)
	@echo ""
	@echo "D-Bus API HTML documentation has been generated in doc/dbus-api/doc/spec"
	@echo ""

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = libs src ringtones man $(TESTS_DIR)
EXTRA_DIST = m4/*.m4 images/* README.gentoo

