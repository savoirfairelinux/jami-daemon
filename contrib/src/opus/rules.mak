# opus

OPUS_VERSION := 1.4 
OPUS_URL := https://github.com/xiph/opus/archive/v$(OPUS_VERSION).tar.gz

PKGS += opus
ifeq ($(call need_pkg,"opus >= 0.9.14"),)
PKGS_FOUND += opus
endif

$(TARBALLS)/opus-$(OPUS_VERSION).tar.gz:
	$(call download,$(OPUS_URL))

.sum-opus: opus-v$(OPUS_VERSION).tar.gz

opus: opus-$(OPUS_VERSION).tar.gz .sum-opus
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.opus: opus toolchain.cmake
	cd $< && $(CMAKE) . && $(MAKE)
	cd $< && $(MAKE) install
	touch $@
