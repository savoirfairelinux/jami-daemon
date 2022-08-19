# libgc

LIBGC_VERSION := 8.0.4
LIBGC_URL     := https://github.com/ivmai/bdwgc/releases/download/v${LIBGC_VERSION}/gc-${LIBGC_VERSION}.tar.gz

DEPS_libgc=

$(TARBALLS)/libgc-$(LIBGC_VERSION).tar.gz:
	$(call download,$(LIBGC_URL))

.sum-libgc: libgc-$(LIBGC_VERSION).tar.gz

libgc: libgc-$(LIBGC_VERSION).tar.gz .sum-libgc
	$(UNPACK)
	mv gc-$(LIBGC_VERSION) libgc-$(LIBGC_VERSION)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

LIBGC_CONF :=             \
	$(HOSTCONF)       \
        --enable-cpluplus \
        --disable-munmap  \
        --enable-shared


.libgc: libgc
	cd $< && $(HOSTVARS) ./configure $(LIBGC_CONF)
	cd $< && $(MAKE) install
	touch $@
