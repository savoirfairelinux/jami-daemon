# C library for curve secp256k1

SECP256K1_VERSION := 0b7024185045a49a1a6a4c5615bf31c94f63d9c4
SECP256K1_URL := https://github.com/bitcoin-core/secp256k1/archive/$(SECP256K1_VERSION).tar.gz

PKGS += secp256k1
ifeq ($(call need_pkg,"libsecp256k1"),)
PKGS_FOUND += secp256k1
endif
DEPS_secp256k1 = gmp $(DEPS_gmp)

$(TARBALLS)/secp256k1-$(SECP256K1_VERSION).tar.gz:
	$(call download,$(SECP256K1_URL))

.sum-secp256k1: secp256k1-$(SECP256K1_VERSION).tar.gz

secp256k1: secp256k1-$(SECP256K1_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.secp256k1: secp256k1 .sum-secp256k1
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-tests --disable-exhaustive-tests
ifeq ($(IOS_TARGET_PLATFORM),iPhoneOS)
	cd $< && $(MAKE) CFLAGS+="-USECP256K1_BUILD -isysroot $(xcrun --sdk iphoneos --show-sdk-path) -arch arm64" install
else
	cd $< && $(MAKE) CFLAGS+="-USECP256K1_BUILD" install
endif
	touch $@
