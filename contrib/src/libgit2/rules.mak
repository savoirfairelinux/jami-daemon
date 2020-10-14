# LIBGIT2
LIBGIT2_VERSION := v1.1.0
LIBGIT2_URL := https://github.com/libgit2/libgit2/archive/${LIBGIT2_VERSION}.tar.gz

PKGS += libgit2
ifeq ($(call need_pkg,"libgit2 >= 1.1.0"),)
PKGS_FOUND += libgit2
endif

DEPS_libgit2 += zlib

$(TARBALLS)/libgit2-$(LIBGIT2_VERSION).tar.gz:
	$(call download,$(LIBGIT2_URL))

.sum-libgit2: libgit2-$(LIBGIT2_VERSION).tar.gz

libgit2: libgit2-$(LIBGIT2_VERSION).tar.gz
	$(UNPACK)
	(cd libgit2-1.1.0 && patch -flp1) < ../../contrib/src/libgit2/debug.patch
	mv libgit2-1.1.0 libgit2
	touch libgit2

.libgit2: libgit2
	cd $< && mkdir -p build && cd build \
    && $(CMAKE) -DCMAKE_C_FLAGS=-fPIC -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DUSE_HTTPS=OFF -DCURL=OFF -DBUILD_CLAR=OFF -DBUILD_SHARED_LIBS=OFF -DUSE_SSH=OFF .. \
    && $(CMAKE) --build . \
    && $(MAKE) install
	touch $@
