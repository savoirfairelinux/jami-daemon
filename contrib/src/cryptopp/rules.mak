# Crypto++
CRYPTOPP_VERSION := 432db09b72c2f8159915e818c5f34dca34e7c5ac
CRYPTOPP_URL := https://github.com/weidai11/cryptopp/archive/$(CRYPTOPP_VERSION).tar.gz

PKGS += cryptopp

ifeq ($(call need_pkg,'libcrypto++'),)
PKGS_FOUND += cryptopp
endif

CRYPTOPP_CMAKECONF := -DBUILD_TESTING=Off \
		-DBUILD_SHARED=Off \
		-DCMAKE_INSTALL_LIBDIR=lib

$(TARBALLS)/cryptopp-$(CRYPTOPP_VERSION).tar.gz:
	$(call download,$(CRYPTOPP_URL))

.sum-cryptopp: cryptopp-$(CRYPTOPP_VERSION).tar.gz

cryptopp: cryptopp-$(CRYPTOPP_VERSION).tar.gz .sum-cryptopp
	$(UNPACK)
	$(APPLY) $(SRC)/cryptopp/cmake-add-BUILD_STATIC_SHARED.patch
	$(MOVE)

.cryptopp: cryptopp toolchain.cmake
	cd $< && rm GNUmakefile*
	cd $< && $(HOSTVARS) $(CMAKE) . $(CRYPTOPP_CMAKECONF)
	cd $< && $(MAKE) install
	touch $@
