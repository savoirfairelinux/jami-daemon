# LIBARCHIVE
LIBARCHIVE_VERSION := 3.7.9
PKG_CPE += cpe:2.3:a:libarchive:libarchive:$(LIBARCHIVE_VERSION):*:*:*:*:*:*:*
LIBARCHIVE_URL := https://github.com/libarchive/libarchive/releases/download/v$(LIBARCHIVE_VERSION)/libarchive-$(LIBARCHIVE_VERSION).tar.xz

ifndef HAVE_MACOSX
PKGS += libarchive
ifeq ($(call need_pkg,"libarchive >= 3.4.0"),)
PKGS_FOUND += libarchive
endif
endif
DEPS_libarchive += nettle gmp zlib

LIBARCHIVE_CMAKECONF := \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DENABLE_TEST=OFF \
		-DENABLE_TAR=OFF \
		-DENABLE_CPIO=OFF \
		-DENABLE_CAT=OFF \
		-DENABLE_LIBXML2=OFF

$(TARBALLS)/libarchive-$(LIBARCHIVE_VERSION).tar.xz:
	$(call download,$(LIBARCHIVE_URL))

.sum-libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.xz

libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.xz .sum-libarchive
	$(UNPACK)
	$(MOVE)

.libarchive: libarchive toolchain.cmake .sum-libarchive
	cd $< && mkdir -p buildlib
ifdef HAVE_ANDROID
	cd $< && cp -R contrib/android/include/* $(PREFIX)/include
endif
	cd $< && cd buildlib && $(HOSTVARS) $(CMAKE) .. $(LIBARCHIVE_CMAKECONF)
	cd $< && cd buildlib && $(MAKE) install
	touch $@
