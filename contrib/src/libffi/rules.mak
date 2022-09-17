# libffi

LIBFFI_VERSION := 3.3
LIBFFI_URL     := ftp://sourceware.org/pub/libffi/libffi-${LIBFFI_VERSION}.tar.gz

DEPS_libffi=

$(TARBALLS)/libffi-$(LIBFFI_VERSION).tar.gz:
	$(call download,$(LIBFFI_URL))

.sum-libffi: libffi-$(LIBFFI_VERSION).tar.gz

libffi: libffi-$(LIBFFI_VERSION).tar.gz .sum-libffi
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

LIBFFI_CONF :=           \
	$(HOSTCONF)      \
	--disable-static \
	--enable-shared

.libffi: libffi
	cd $< && $(HOSTVARS) ./configure $(LIBFFI_CONF)
	cd $< && $(MAKE) install
	touch $@
