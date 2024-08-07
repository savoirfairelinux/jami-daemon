# ZLIB
ZLIB_VERSION := 1.2.8
PKG_CPE += cpe:2.3:a:zlib:zlib:$(ZLIB_VERSION):*:*:*:*:*:*:*
ZLIB_URL := https://github.com/madler/zlib/archive/v$(ZLIB_VERSION).tar.gz

PKGS += zlib
ifndef HAVE_IOS
ifeq ($(call need_pkg,"zlib"),)
PKGS_FOUND += zlib
endif
else
PKGS_FOUND += zlib
endif

ifeq ($(shell uname),Darwin) # zlib tries to use libtool on Darwin
ifdef HAVE_CROSS_COMPILE
ZLIB_CONFIG_VARS=CHOST=$(HOST)
endif
endif

$(TARBALLS)/zlib-$(ZLIB_VERSION).tar.gz:
	$(call download,$(ZLIB_URL))

.sum-zlib: zlib-$(ZLIB_VERSION).tar.gz

zlib: zlib-$(ZLIB_VERSION).tar.gz .sum-zlib
	$(UNPACK)
	$(MOVE)

.zlib: zlib
	cd $< && $(HOSTVARS) $(ZLIB_CONFIG_VARS) ./configure --prefix=$(PREFIX) --static
	cd $< && $(MAKE) install
	touch $@
