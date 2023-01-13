# LIBGIT2
LIBGIT2_VERSION := 1.1.0
LIBGIT2_URL := https://github.com/libgit2/libgit2/archive/v${LIBGIT2_VERSION}.tar.gz

PKGS += libgit2
ifeq ($(call need_pkg,"libgit2 >= 1.1.0"),)
PKGS_FOUND += libgit2
endif

DEPS_libgit2 += zlib http_parser

$(TARBALLS)/libgit2-v$(LIBGIT2_VERSION).tar.gz:
	$(call download,$(LIBGIT2_URL))

.sum-libgit2: libgit2-v$(LIBGIT2_VERSION).tar.gz

libgit2: libgit2-v$(LIBGIT2_VERSION).tar.gz .sum-libgit2
	$(UNPACK)
	mv libgit2-$(LIBGIT2_VERSION) libgit2-v$(LIBGIT2_VERSION)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

# TODO windows pcre?
.libgit2: libgit2
	cd $< && mkdir -p build && cd build \
    && $(CMAKE) -DCMAKE_C_FLAGS=-fPIC -DUSE_HTTPS=OFF -DCURL=OFF -DUSE_HTTP_PARSER=system -DBUILD_CLAR=OFF -DBUILD_SHARED_LIBS=OFF -DUSE_SSH=OFF -DREGEX_BACKEND=regcomp .. \
    && $(MAKE) install
	touch $@
