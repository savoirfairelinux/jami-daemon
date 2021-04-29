# LIBARCHIVE
LIBARCHIVE_VERSION := 3.4.0
LIBARCHIVE_URL := https://github.com/libarchive/libarchive/releases/download/v$(LIBARCHIVE_VERSION)/libarchive-$(LIBARCHIVE_VERSION).tar.gz

ifndef HAVE_MACOSX
PKGS += libarchive
ifeq ($(call need_pkg,"libarchive >= 3.4.0"),)
PKGS_FOUND += libarchive
else
DEPS_libarchive += nettle
endif
endif

LIBARCHIVE_CMAKECONF := \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DENABLE_TEST=OFF \
		-DENABLE_TAR=OFF \
		-DENABLE_CPIO=OFF \
		-DENABLE_CAT=OFF \
		-DENABLE_LIBXML2=OFF

$(TARBALLS)/libarchive-$(LIBARCHIVE_VERSION).tar.gz:
	$(call download,$(LIBARCHIVE_URL))

.sum-libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz

libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.libarchive: libarchive toolchain.cmake .sum-libarchive
	cd $< && mkdir -p buildlib
ifdef HAVE_ANDROID
	cd $< && cp -R contrib/android/include/* $(PREFIX)/include
endif
	cd $< && cd buildlib && $(HOSTVARS) $(CMAKE) .. $(LIBARCHIVE_CMAKECONF)
	cd $< && cd buildlib && $(MAKE) install
ifdef HAVE_LINUX
	cd $< && cd $(PREFIX)/lib && rm libarchive.so*
else ifdef HAVE_DARWIN_OS
	cd $< && cd $(PREFIX)/lib && rm libarchive*.dylib
endif
	touch $@
