# MINIZIP
LIBMINIZIP_VERSION := 3.0.0
LIBMINIZIP_URL := https://github.com/zlib-ng/minizip-ng/archive/refs/tags/$(LIBMINIZIP_VERSION).tar.gz

PKGS += minizip
ifeq ($(call need_pkg,"minizip >= 3.0.0"),)
PKGS_FOUND += minizip
endif

DEPS_minizip = zlib

LIBMINIZIP_CMAKECONF := \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DMZ_ICONV=OFF \
		-DMZ_BZIP2=OFF \
		-DMZ_BZIP2=OFF \
		-DMZ_LZMA=OFF \
		-DMZ_ZSTD=OFF \
		-DMZ_LIBCOMP=OFF \
		-DMZ_FETCH_LIBS=OFF \
		-DMZ_FORCE_FETCH_LIBS=OFF \
		-DMZ_WZAES=OFF \
		-DMZ_OPENSSL=OFF \
		-DMZ_LIBBSD=OFF \
		-DMZ_SIGNING=OFF


$(TARBALLS)/minizip-ng-$(LIBMINIZIP_VERSION).tar.gz:
	$(call download,$(LIBMINIZIP_URL))

.sum-minizip: minizip-ng-$(LIBMINIZIP_VERSION).tar.gz

minizip: minizip-ng-$(LIBMINIZIP_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.minizip: minizip toolchain.cmake .sum-minizip
	cd $< && mkdir -p buildlib
ifdef HAVE_ANDROID
	cd $< && cp -R contrib/android/include/* $(PREFIX)/include
endif
	cd $< && cd buildlib && $(HOSTVARS) $(CMAKE) .. $(LIBMINIZIP_CMAKECONF)
	cd $< && cd buildlib && $(MAKE) install
	cd $< && rm -r buildlib
	touch $@
