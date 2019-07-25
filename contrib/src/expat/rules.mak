EXPAT_VERSION := 2.2.7
EXPAT_URL := https://github.com/libexpat/libexpat/releases/download/R_2_2_7/expat-2.2.7.tar.gz

ifeq ($(call need_pkg,"expat >= 2.2.7"),)
PKGS_FOUND += expat
endif

$(TARBALLS)/expat-${EXPAT_VERSION}.tar.gz:
	$(call download,$(EXPAT_URL))

.sum-expat: expat-${EXPAT_VERSION}.tar.gz

expat: expat-${EXPAT_VERSION}.tar.gz .sum-expat
	$(UNPACK)
	$(MOVE)

.expat: expat
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-shared --without-examples --without-tests
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@
