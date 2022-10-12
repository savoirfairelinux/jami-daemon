# C library for curve secp256k1

SECP256K1_VERSION := 0b7024185045a49a1a6a4c5615bf31c94f63d9c4
SECP256K1_URL := https://github.com/bitcoin-core/secp256k1/archive/$(SECP256K1_VERSION).tar.gz

PKGS += secp256k1
ifeq ($(call need_pkg,"libsecp256k1"),)
PKGS_FOUND += secp256k1
endif
DEPS_secp256k1 = gmp

$(TARBALLS)/secp256k1-$(SECP256K1_VERSION).tar.gz:
	$(call download,$(SECP256K1_URL))

.sum-secp256k1: secp256k1-$(SECP256K1_VERSION).tar.gz

secp256k1: secp256k1-$(SECP256K1_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

ifdef HAVE_IOS
SECP256K1_CFLAGS := -USECP256K1_BUILD $(CFLAGS)
else
ifdef HAVE_MACOSX
ifeq ($(ARCH),arm64)
SECP256K1_CFLAGS := -USECP256K1_BUILD $(CFLAGS)
else
SECP256K1_CFLAGS := -USECP256K1_BUILD
endif
else
SECP256K1_CFLAGS := -USECP256K1_BUILD
endif
endif

.secp256k1: secp256k1 .sum-secp256k1
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-tests --disable-exhaustive-tests
	cd $< && $(MAKE) CFLAGS+='$(SECP256K1_CFLAGS)' install

	touch $@
