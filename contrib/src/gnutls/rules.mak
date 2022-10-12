# GnuTLS

GNUTLS_VERSION := 3.7.6
GNUTLS_URL := https://www.gnupg.org/ftp/gcrypt/gnutls/v3.7/gnutls-$(GNUTLS_VERSION).tar.xz

PKGS += gnutls

ifeq ($(call need_pkg,"gnutls >= 3.6.7"),)
PKGS_FOUND += gnutls
endif

$(TARBALLS)/gnutls-$(GNUTLS_VERSION).tar.xz:
	$(call download,$(GNUTLS_URL))

.sum-gnutls: gnutls-$(GNUTLS_VERSION).tar.xz

gnutls: gnutls-$(GNUTLS_VERSION).tar.xz .sum-gnutls
	$(UNPACK)
	$(APPLY) $(SRC)/gnutls/0001-m4-remove-malloc-realloc.patch
ifndef HAVE_IOS
	$(APPLY) $(SRC)/gnutls/mac-keychain-lookup.patch
endif
	$(call pkg_static,"lib/gnutls.pc.in")
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

GNUTLS_CONF := \
	--disable-gtk-doc \
	--without-p11-kit \
	--disable-cxx \
	--disable-srp-authentication \
	--with-included-libtasn1 \
	--with-included-unistring \
	--disable-openssl-compatibility \
	--disable-guile \
	--disable-nls \
	--disable-dtls-srtp-support \
	--without-libintl-prefix \
	--without-idn \
	$(HOSTCONF)

ifdef HAVE_MACOSX
	GNUTLS_CONF += --without-brotli
#ifeq ($(ARCH),arm64)
#	GNUTLS_CONF += --disable-hardware-acceleration
#endif
endif

ifdef HAVE_IOS
	GNUTLS_CONF += \
	--disable-hardware-acceleration \
	--without-brotli \
	--without-zstd
endif

DEPS_gnutls = gmp nettle iconv


#Workaround for localtime_r function
ifdef HAVE_WIN32
CFLAGS="-D_POSIX_C_SOURCE"
endif

.gnutls: gnutls
	$(RECONF)
ifdef HAVE_ANDROID
	cd $< && $(HOSTVARS) ./configure $(GNUTLS_CONF)
else
ifdef HAVE_IOS
	cd $< && $(HOSTVARS) ac_cv_func_clock_gettime=no ./configure $(GNUTLS_CONF)
else
ifdef HAVE_MACOSX
	cd $< && $(HOSTVARS) ac_cv_func_clock_gettime=no ./configure $(GNUTLS_CONF)
else
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS)" ./configure $(GNUTLS_CONF)
endif
endif
endif
	cd $</gl && $(MAKE) install
	cd $</lib && $(MAKE) install
	touch $@
