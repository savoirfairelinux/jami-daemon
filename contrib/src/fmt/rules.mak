# FMT
FMT_VERSION := 5.3.0
FMT_URL := https://github.com/fmtlib/fmt/archive/$(FMT_VERSION).tar.gz

PKGS += fmt
ifeq ($(call need_pkg,'fmt'),)
PKGS_FOUND += fmt
endif

# fmt 5.3.0 fix: https://github.com/fmtlib/fmt/issues/1267
FMT_CMAKECONF = -DBUILD_SHARED_LIBS=Off -DFMT_USE_USER_DEFINED_LITERALS=0 \
                CMAKE_INSTALL_LIBDIR=$(PREFIX)/lib

$(TARBALLS)/fmt-$(FMT_VERSION).tar.gz:
	$(call download,$(FMT_URL))

.sum-fmt: fmt-$(FMT_VERSION).tar.gz

fmt: fmt-$(FMT_VERSION).tar.gz
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.fmt: fmt toolchain.cmake .sum-fmt
	cd $< && $(HOSTVARS) $(CMAKE) $(FMT_CMAKECONF) .
	cd $< && $(MAKE) install
	cd $< && mv $(PREFIX)/share/pkgconfig/fmt.pc $(PREFIX)/lib/pkgconfig/
	touch $@
