# LZMA
LZMA_VERSION := 1514
LZMA_URL := http://www.7-zip.org/a/lzma$(LZMA_VERSION).7z

ifeq ($(call need_pkg,'lzma'),)
PKGS += lzma
endif

PKGS_FOUND += lzma

LZMA_CMAKECONF := -DBUILD_STATIC_LIBS:BOOL=ON \
                     -DBUILD_SHARED_LIBS:BOOL=OFF \
                     -DLZMA_WITH_TESTS:BOOL=OFF

$(TARBALLS)/lzma$(LZMA_VERSION).7z:
	$(call download,$(LZMA_URL))

.sum-lzma: lzma$(LZMA_VERSION).7z

lzma: lzma$(LZMA_VERSION).7z .sum-lzma
	$(UNPACK)
	$(MOVE)

.lzma: lzma toolchain.cmake
	cd $<
	cd CPP/7zip/Bundles/LzmaCon && make -f makefile.gcc clean all
	touch $@
