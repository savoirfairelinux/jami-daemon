include globals.mk

# Makefile.am

SOURCES=

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src ringtones man doc bin test

EXTRA_DIST = m4/libtool.m4 \
			 m4/lt~obsolete.m4 \
			 m4/ltoptions.m4 \
			 m4/ltsugar.m4 \
			 m4/ltversion.m4 \
			 contrib/bootstrap \
			 contrib/src

precoverage:
	$(AM_V_at)lcov --no-external --capture --initial --directory $(top_builddir) --output-file dring-coverage-base.info

coverage: precoverage check
	$(AM_V_at)lcov --no-external --capture --directory $(top_builddir) --output-file dring-coverage-tests.info
	$(AM_V_at)lcov --add-tracefile dring-coverage-base.info --add-tracefile dring-coverage-tests.info --output-file dring-coverage.info
	$(AM_V_at)lcov --remove dring-coverage.info '/contrib/*' '/usr/include/*' '/bin/*' '/foo/bin/*' '/foo/contrib/*' --output-file dring-coverage-filtered.info
	$(MKDIR_P) html-output
	genhtml -o html-output dring-coverage-filtered.info

coverage-clean:
	$(AM_V_at)lcov --directory $(top_builddir) --zerocounters
	$(AM_V_at)rm -rf dring-coverage-filtered.info
	$(AM_V_at)rm -rf html-output
	$(AM_V_at)find -name '*.gcda' -exec rm -f {} +
	$(AM_V_at)find -name '*.gcno' -exec rm -f {} +

.PHONY: precoverage coverage coverage-clean
