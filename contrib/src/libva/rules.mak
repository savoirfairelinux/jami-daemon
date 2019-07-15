# LIBVA
LIBVA_VERSION := 2.5.0
LIBVA_URL := https://github.com/intel/libva/archive/$(LIBVA_VERSION).tar.gz

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
# Desktop Linux

ifeq ($(call need_pkg,"libva >= 1.5.0"),)
PKGS_FOUND += libva
endif

LIBVA_CONF := --enable-drm \
			  --enable-x11 \
			  --enable-glx \
			  --enable-wayland \


$(TARBALLS)/libva-$(LIBVA_VERSION).tar.gz:
	$(call download,$(LIBVA_URL))

.sum-libva: libva-$(LIBVA_VERSION).tar.gz

libva: libva-$(LIBVA_VERSION).tar.gz .sum-libva
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.libva: libva
	cd $< && ./autogen.sh
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS)" ./configure $(LIBVA_CONF) $(HOSTCONF) --enable-shared
	cd $< && $(MAKE) install
	touch $@

endif
endif
