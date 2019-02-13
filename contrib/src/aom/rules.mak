AOM_VERSION := v1.0.0
AOM_URL := https://aomedia.googlesource.com/aom/+archive/$(AOM_VERSION).tar.gz

PKGS += aom

ifeq ($(call need_pkg,"aom >= 1.0.0"),)
PKGS_FOUND += aom
endif

AOM_CMAKECONF := -DBUILD_SHARED_LIBS:BOOL=OFF

$(TARBALLS)/aom-$(AOM_VERSION).tar.gz:
	$(call download,$(AOM_URL))

.sum-aom: aom-$(AOM_VERSION).tar.gz

aom: aom-$(AOM_VERSION).tar.gz .sum-aom
	mkdir aom-$(AOM_VERSION)
	tar xzf $(TARBALLS)/aom-$(AOM_VERSION).tar.gz -C aom-$(AOM_VERSION)
	$(MOVE)

.aom: aom toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) ${AOM_CMAKECONF}
	cd $< && $(MAKE) install
	touch $@
