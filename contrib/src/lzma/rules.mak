# LZMA
LZMA_VERSION := 1514
LZMA_URL := http://www.7-zip.org/a/lzma$(LZMA_VERSION).7z

PKGS += lzma

ifeq ($(call need_pkg,'lzma'),)
PKGS_FOUND += lzma
endif

LZMA_CMAKECONF := -DBUILD_STATIC_LIBS:BOOL=ON \
                     -DBUILD_SHARED_LIBS:BOOL=OFF \
                     -DLZMA_WITH_TESTS:BOOL=OFF

$(TARBALLS)/jsoncpp-$(LZMA_VERSION).tar.gz:
	$(call download,$(LZMA_URL))

.sum-jsoncpp: jsoncpp-$(LZMA_VERSION).tar.gz

jsoncpp: jsoncpp-$(LZMA_VERSION).tar.gz .sum-jsoncpp
	$(UNPACK)
	$(MOVE)

.jsoncpp: jsoncpp toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) . ${LZMA_CMAKECONF}
	cd $< && $(MAKE) install
	touch $@
