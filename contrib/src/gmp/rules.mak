# GNU Multiple Precision Arithmetic

GMP_VERSION := 6.1.0
GMP_URL := https://gmplib.org/download/gmp-$(GMP_VERSION)/gmp-$(GMP_VERSION).tar.bz2

$(TARBALLS)/gmp-$(GMP_VERSION).tar.bz2:
	$(call download,$(GMP_URL))

.sum-gmp: gmp-$(GMP_VERSION).tar.bz2

gmp: gmp-$(GMP_VERSION).tar.bz2 .sum-gmp
	$(UNPACK)
	$(MOVE)

.gmp: gmp
ifdef HAVE_IOS
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -O3" ./configure --disable-assembly $(HOSTCONF)
else
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
endif
	cd $< && $(MAKE) install
	touch $@
