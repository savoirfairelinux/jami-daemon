# FMT
FMT_VERSION := 5.3.0
FMT_URL := https://github.com/fmtlib/fmt/archive/$(FMT_VERSION).tar.gz

PKGS += fmt
ifeq ($(call need_pkg,'fmt'),)
PKGS_FOUND += fmt
endif

$(TARBALLS)/fmt-$(FMT_VERSION).tar.gz:
	$(call download,$(FMT_URL))

.sum-fmt: fmt-$(FMT_VERSION).tar.gz

fmt: fmt-$(FMT_VERSION).tar.gz
	$(UNPACK)
	cd $(UNPACK_DIR)
	$(MOVE)

.fmt: fmt toolchain.cmake .sum-fmt
	cd $< && $(HOSTVARS) $(CMAKE) .
	cd $< && $(MAKE) install
	touch $@
