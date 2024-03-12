# LIBGIT2
LIBGIT2_VERSION := 1.6.4
LIBGIT2_URL := https://github.com/libgit2/libgit2/archive/v${LIBGIT2_VERSION}.tar.gz

PKGS += libgit2
ifeq ($(call need_pkg,"libgit2 >= 1.6.0"),)
PKGS_FOUND += libgit2
endif

DEPS_libgit2 += zlib http_parser

$(TARBALLS)/libgit2-v$(LIBGIT2_VERSION).tar.gz:
	$(call download,$(LIBGIT2_URL))

.sum-libgit2: libgit2-v$(LIBGIT2_VERSION).tar.gz

libgit2: libgit2-v$(LIBGIT2_VERSION).tar.gz .sum-libgit2
	$(UNPACK)
	mv libgit2-$(LIBGIT2_VERSION) libgit2-v$(LIBGIT2_VERSION)
	$(APPLY) $(SRC)/libgit2/0001-fix-getentropy.patch
	$(APPLY) $(SRC)/libgit2/c-standard.diff
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

LIBGIT2_CONF = -DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF \
		-DBUILD_TESTS=OFF \
		-DBUILD_CLI=OFF \
		-DUSE_HTTPS=OFF \
		-DCURL=OFF \
		-DUSE_HTTP_PARSER=system \
		-DBUILD_CLAR=OFF \
		-DUSE_SSH=OFF \
		-DREGEX_BACKEND=regcomp \
		-DCMAKE_C_FLAGS="-fPIC"

# TODO windows pcre?
.libgit2: libgit2
	cd $< && mkdir -p build && cd build \
    && $(CMAKE) $(LIBGIT2_CONF) .. \
    && $(MAKE) install
	touch $@
