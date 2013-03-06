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

coverage:
	$(AM_V_at)lcov --directory $(top_builddir) --capture --output-file sflphone-coverage.info
	$(MKDIR_P) html-output
	genhtml -o html-output sflphone-coverage.info

coverage-clean:
	$(AM_V_at)lcov --directory $(top_builddir) --zerocounters
	$(AM_V_at)rm -rf sflphone-coverage.info
	$(AM_V_at)rm -rf html-output
	$(AM_V_at)find -name '*.gcda' -exec rm -f {} +
	$(AM_V_at)find -name '*.gcno' -exec rm -f {} +

.PHONY: coverage coverage-clean
