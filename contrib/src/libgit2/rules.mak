# LIBGIT2
LIBGIT2_VERSION := v0.27.9
LIBGIT2_URL := https://github.com/libgit2/libgit2/archive/${LIBGIT2_VERSION}.tar.gz

PKGS += libgit2
ifeq ($(call need_pkg,"libgit2 >= 0.27.9"),)
PKGS_FOUND += libgit2
endif

DEPS_libgit2 += zlib

$(TARBALLS)/libgit2-$(LIBGIT2_VERSION).tar.gz:
	$(call download,$(LIBGIT2_URL))

.sum-libgit2: libgit2-$(LIBGIT2_VERSION).tar.gz

libgit2: libgit2-$(LIBGIT2_VERSION).tar.gz
	$(UNPACK)
	mv libgit2-0.27.9 libgit2 && touch libgit2

.libgit2: libgit2
	cd $< && mkdir -p build && cd build \
    && $(CMAKE) -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DUSE_HTTPS=OFF -DCURL=OFF -DBUILD_CLAR=OFF -DBUILD_SHARED_LIBS=OFF -DUSE_SSH=OFF .. \
    && $(CMAKE) --build . \
    && $(MAKE) install
	touch $@
