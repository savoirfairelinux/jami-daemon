MMCOMMON_VERSION := 0.9.12
MMCOMMON_URL := https://gitlab.gnome.org/GNOME/mm-common/-/archive/$(MMCOMMON_VERSION)/mm-common-$(MMCOMMON_VERSION).tar.gz

ifeq ($(call need_pkg,'mm-common-libstdc++ >= 0.9.12'),)
PKGS_FOUND += mm-common
endif

$(TARBALLS)/mm-common-$(MMCOMMON_VERSION).tar.gz:
	$(call download,$(MMCOMMON_URL))

.sum-mm-common: mm-common-$(MMCOMMON_VERSION).tar.gz

mm-common: mm-common-$(MMCOMMON_VERSION).tar.gz .sum-mm-common
	$(UNPACK)
	$(MOVE)

.mm-common: mm-common
	cd $< && NOCONFIGURE=1 ./autogen.sh
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-network && $(MAKE) && $(MAKE) install
	touch $@
