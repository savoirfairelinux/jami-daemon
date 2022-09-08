NOWIDE_VERSION := 11.2.0
NOWIDE_URL := https://github.com/boostorg/nowide/releases/download/v$(NOWIDE_VERSION)/nowide_standalone_v$(NOWIDE_VERSION).tar.gz

PKGS += nowide

NOWIDE_CMAKECONF := -DNOWIDE_INSTALL=ON \
                    -DBUILD_TESTING=OFF

$(TARBALLS)/nowide_standalone_v$(NOWIDE_VERSION).tar.gz:
	$(call download,$(NOWIDE_URL))

.sum-nowide: nowide_standalone_v$(NOWIDE_VERSION).tar.gz

nowide: nowide_standalone_v$(NOWIDE_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.nowide: nowide toolchain.cmake .sum-nowide
	cd $< && $(HOSTVARS) $(CMAKE) . $(NOWIDE_CMAKECONF)
	cd $< && $(MAKE) install
	touch $@
