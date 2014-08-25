# GNU Cryptography

GCRYPT_VERSION := 1.6.1
GCRYPT_URL := ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-$(GCRYPT_VERSION).tar.bz2

ifeq ($(call need_pkg," >= 1.5.0"),)
PKGS_FOUND += gcrypt
endif

DEPS_gcrypt = gpgerror

$(TARBALLS)/libgcrypt-$(GCRYPT_VERSION).tar.bz2:
	$(call download,$(GCRYPT_URL))

.sum-gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2

gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2 .sum-gcrypt
	$(UNPACK)
	$(APPLY) $(SRC)/gcrypt/gcrypt-fix-x86_64-codepath-on-Darwin.patch
	$(APPLY) $(SRC)/gcrypt/fix-amd64-assembly-on-solaris.patch
	$(APPLY) $(SRC)/gcrypt/0001-Fix-assembly-division-check.patch
	$(APPLY) $(SRC)/gcrypt/mpi-darwin13.patch
	$(MOVE)


GCRYPT_CONF = \
	--enable-ciphers=aes,des,rfc2268,arcfour \
	--enable-digests=sha1,md5,rmd160,sha256,sha512 \
	--enable-pubkey-ciphers=dsa,rsa,ecc
ifdef HAVE_WIN64
GCRYPT_CONF += --disable-asm
endif
ifdef HAVE_IOS
GCRYPT_EXTRA_CFLAGS = -fheinous-gnu-extensions
else
GCRYPT_EXTRA_CFLAGS =
endif
ifdef HAVE_MACOSX
GCRYPT_CONF += --disable-aesni-support
else
ifdef HAVE_BSD
GCRYPT_CONF += --disable-asm --disable-aesni-support
endif
endif
ifdef HAVE_ANDROID
ifeq ($(ANDROID_ABI), x86)
GCRYPT_CONF += ac_cv_sys_symbol_underscore=no
endif
endif

.gcrypt: gcrypt
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) $(GCRYPT_EXTRA_CFLAGS)" ./configure $(HOSTCONF) $(GCRYPT_CONF)
	cd $< && $(MAKE) install
	touch $@
