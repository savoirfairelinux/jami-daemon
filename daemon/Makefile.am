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

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = libs src ringtones man $(TESTS_DIR) doc

EXTRA_DIST = m4/libtool.m4 \
			 m4/lt~obsolete.m4 \
			 m4/ltoptions.m4 \
			 m4/ltsugar.m4 \
			 m4/ltversion.m4
