# ZRTPCPP
ZRTPCPP_VERSION := 2.3.4
ZRTPCPP_URL := $(GNUTELEPHONY)/libzrtpcpp-$(ZRTPCPP_VERSION).tar.gz

PKGS += zrtpcpp
ifeq ($(call need_pkg,'libzrtpcpp >= 2.0.0'),)
PKGS_FOUND += zrtpcpp
endif

DEPS_zrtpcpp = ccrtp ucommon gcrypt

ZRTPCPP_CMAKECONF := -DBUILD_STATIC:BOOL=ON \
                     -DBUILD_SHARED:BOOL=OFF \
                     -DBUILD_SHARED_LIBS:BOOL=OFF

$(TARBALLS)/libzrtpcpp-$(ZRTPCPP_VERSION).tar.gz:
	$(call download,$(ZRTPCPP_URL))

.sum-zrtpcpp: libzrtpcpp-$(ZRTPCPP_VERSION).tar.gz

zrtpcpp: libzrtpcpp-$(ZRTPCPP_VERSION).tar.gz .sum-zrtpcpp
	$(UNPACK)
	$(APPLY) $(SRC)/zrtpcpp/forcegcrypt.patch
	$(APPLY) $(SRC)/zrtpcpp/threadcbsupdate.patch
	$(APPLY) $(SRC)/zrtpcpp/standardheader.patch
	$(MOVE)

.zrtpcpp: zrtpcpp toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) . $(ZRTPCPP_CMAKECONF)
	cd $< && $(MAKE) install VERBOSE=1
	touch $@
