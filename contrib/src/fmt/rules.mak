# FMT
FMT_VERSION := 9.0.0
FMT_URL := https://github.com/fmtlib/fmt/archive/$(FMT_VERSION).tar.gz

PKGS += fmt
ifeq ($(call need_pkg,'fmt >= 6.0'),)
PKGS_FOUND += fmt
endif

# fmt 5.3.0 fix: https://github.com/fmtlib/fmt/issues/1267
FMT_CMAKECONF = -DBUILD_SHARED_LIBS=Off \
				-DFMT_TEST=Off \
                CMAKE_INSTALL_LIBDIR=$(PREFIX)/lib

$(TARBALLS)/fmt-$(FMT_VERSION).tar.gz:
	$(call download,$(FMT_URL))

.sum-fmt: fmt-$(FMT_VERSION).tar.gz

fmt: fmt-$(FMT_VERSION).tar.gz
	$(UNPACK)
	cd $(UNPACK_DIR)
	$(MOVE)

.fmt: fmt toolchain.cmake .sum-fmt
	cd $< && $(HOSTVARS) $(CMAKE) $(FMT_CMAKECONF) .
	cd $< && $(MAKE) install
	touch $@
