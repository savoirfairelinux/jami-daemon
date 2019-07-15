# LIBVA
LIBVA_VERSION := 2.5.0
LIBVA_URL := https://github.com/intel/libva/archive/$(LIBVA_VERSION).tar.gz

PKGS += libva
ifeq ($(call need_pkg,"libva >= 2.5.0"),)
PKGS_FOUND += libva
endif

$(TARBALLS)/libva-$(LIBVA_VERSION).tar.gz:
	$(call download,$(LIBVA_URL))

.sum-libva: libva-$(LIBVA_VERSION).tar.gz

libva: libva-$(LIBVA_VERSION).tar.gz .sum-libva
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.libva: libva
	cd $< && ./autogen.sh
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS)" ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@