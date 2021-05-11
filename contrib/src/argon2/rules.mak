# ARGON2
ARGON2_VERSION := 1eea0104e7cb2a38c617cf90ffa46ce5db6aceda
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
	$(APPLY) $(SRC)/argon2/pkgconfig.patch
	$(MOVE)

.argon2: argon2 .sum-argon2
	cd $< && sed -i'.orig' -e 's|@PREFIX@|$(PREFIX)|' -e "s|@HOST_MULTIARCH@||" -e "s|@UPSTREAM_VER@|$(ARGON2_VERSION)|" libargon2.pc
	cd $< && mkdir -p $(PREFIX)/lib/pkgconfig/ && cp libargon2.pc $(PREFIX)/lib/pkgconfig/
	cd $< && $(HOSTVARS) $(MAKE) libs PREFIX="$(PREFIX)" OPTTARGET="no-opt" LIB_SH=""
	cd $< && $(RANLIB) libargon2.a
	cd $< && $(HOSTVARS) $(MAKE) install PREFIX="$(PREFIX)" OPTTARGET="no-opt" LIB_SH=""
	rm -f $(PREFIX)/lib/libargon2.so* $(PREFIX)/lib/libargon2*.dylib
	touch $@
