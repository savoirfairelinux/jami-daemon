# Crypto++
CRYPTOPP_VERSION := 54557b18275053bbfc34594f7e65808dd92dd1a6
CRYPTOPP_URL := https://github.com/weidai11/cryptopp/archive/$(CRYPTOPP_VERSION).tar.gz

PKGS += cryptopp

# Debian/Ubuntu
ifeq ($(call need_pkg,'libcrypto++'),)
PKGS_FOUND += cryptopp
else
# Redhat/Fedora
ifeq ($(call need_pkg,'cryptopp'),)
PKGS_FOUND += cryptopp
endif
endif

CRYPTOPP_CMAKECONF := -DBUILD_TESTING=Off \
		-DBUILD_SHARED=Off \
		-DCMAKE_INSTALL_LIBDIR=lib

$(TARBALLS)/cryptopp-$(CRYPTOPP_VERSION).tar.gz:
	$(call download,$(CRYPTOPP_URL))

.sum-cryptopp: cryptopp-$(CRYPTOPP_VERSION).tar.gz

cryptopp: cryptopp-$(CRYPTOPP_VERSION).tar.gz .sum-cryptopp
	$(UNPACK)
ifdef HAVE_ANDROID
	$(APPLY_BIN) $(SRC)/cryptopp/cmake-crosscompile.patch
endif
	$(MOVE)

.cryptopp: cryptopp toolchain.cmake
	cd $< && rm GNUmakefile*
	cd $< && $(HOSTVARS) $(CMAKE) . $(CRYPTOPP_CMAKECONF)
	cd $< && $(MAKE) install
	touch $@
