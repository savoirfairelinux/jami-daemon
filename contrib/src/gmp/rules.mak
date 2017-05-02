# GNU Multiple Precision Arithmetic

GMP_VERSION := 6.1.0
GMP_URL := ftp://ftp.gnu.org/gnu/gmp/gmp-$(GMP_VERSION).tar.bz2

$(TARBALLS)/gmp-$(GMP_VERSION).tar.bz2:
	$(call download,$(GMP_URL))

.sum-gmp: gmp-$(GMP_VERSION).tar.bz2

gmp: gmp-$(GMP_VERSION).tar.bz2 .sum-gmp
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
	cd $< && $(HOSTVARS) ./configure --without-clock-gettime $(HOSTCONF)
else
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
endif
endif
	cd $< && $(MAKE) install
	touch $@
