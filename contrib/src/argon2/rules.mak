# ARGON2
ARGON2_VERSION := 16d3df698db2486dde480b09a732bf9bf48599f9
ARGON2_URL := https://github.com/P-H-C/phc-winner-argon2/archive/$(ARGON2_VERSION).tar.gz

ifeq ($(call need_pkg,'libargon2 > 20161029'),)
PKGS_FOUND += argon2
endif

$(TARBALLS)/argon2-$(ARGON2_VERSION).tar.gz:
	$(call download,$(ARGON2_URL))

.sum-argon2: argon2-$(ARGON2_VERSION).tar.gz

argon2: argon2-$(ARGON2_VERSION).tar.gz
	$(UNPACK)
	mv phc-winner-argon2-$(ARGON2_VERSION) argon2-$(ARGON2_VERSION)
	$(APPLY) $(SRC)/argon2/0001-build-don-t-force-AR-path.patch
	$(MOVE)

ARGON2_CONF = \
	PREFIX="$(PREFIX)" \
	OPTTARGET="no-opt" \
	LIB_SH="" \
	ARGON2_VERSION="20190702"

.argon2: argon2 .sum-argon2
	cd $< && $(HOSTVARS) $(MAKE) libs $(ARGON2_CONF)
	cd $< && $(RANLIB) libargon2.a
	cd $< && $(HOSTVARS) $(MAKE) install $(ARGON2_CONF)
	rm -f $(PREFIX)/lib/libargon2.so* $(PREFIX)/lib/libargon2*.dylib
	touch $@
