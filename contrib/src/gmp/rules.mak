# GNU Multiple Precision Arithmetic


GMP_VERSION := 6.3.0
PKG_CPE += cpe:2.3:a:gmplib:gmp:$(GMP_VERSION):*:*:*:*:*:*:*
GMP_URL := $(GNU)/gmp/gmp-$(GMP_VERSION).tar.xz

ifeq ($(call need_pkg,'gmp >= 6.2.0'),)
PKGS_FOUND += gmp
endif

$(TARBALLS)/gmp-$(GMP_VERSION).tar.xz:
	$(call download,$(GMP_URL))

.sum-gmp: gmp-$(GMP_VERSION).tar.xz

gmp: gmp-$(GMP_VERSION).tar.xz .sum-gmp
	$(UNPACK)
ifdef HAVE_IOS
	$(APPLY) $(SRC)/gmp/clock_gettime.patch
endif
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/gmp/clock_gettime.patch
endif
	$(MOVE)

.gmp: gmp
ifdef HAVE_IOS
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -O3" ./configure --disable-assembly --without-clock-gettime $(HOSTCONF)
else
ifdef HAVE_MACOSX
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure --without-clock-gettime --enable-static --disable-shared $(HOSTCONF)
else
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
endif
endif
	cd $< && $(MAKE) install
	touch $@
