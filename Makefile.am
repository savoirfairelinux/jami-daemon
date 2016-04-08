include globals.mak

# Makefile.am

SOURCES=

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src ringtones man $(TESTS_DIR) doc bin

EXTRA_DIST = m4/libtool.m4 \
			 m4/lt~obsolete.m4 \
			 m4/ltoptions.m4 \
			 m4/ltsugar.m4 \
			 m4/ltversion.m4 \
			 contrib/bootstrap \
			 contrib/src

coverage:
	$(AM_V_at)lcov --directory $(top_builddir) --capture --output-file dring-coverage.info
	$(MKDIR_P) html-output
	genhtml -o html-output dring-coverage.info

coverage-clean:
	$(AM_V_at)lcov --directory $(top_builddir) --zerocounters
	$(AM_V_at)rm -rf dring-coverage.info
	$(AM_V_at)rm -rf html-output
	$(AM_V_at)find -name '*.gcda' -exec rm -f {} +
	$(AM_V_at)find -name '*.gcno' -exec rm -f {} +

.PHONY: coverage coverage-clean
