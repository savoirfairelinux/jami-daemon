# GnuTLS

GNUTLS_VERSION := 3.4.14
GNUTLS_URL := ftp://ftp.gnutls.org/gcrypt/gnutls/v3.4/gnutls-$(GNUTLS_VERSION).tar.xz

PKGS += gnutls

ifeq ($(call need_pkg,"gnutls >= 3.4.14"),)
PKGS_FOUND += gnutls
endif

$(TARBALLS)/gnutls-$(GNUTLS_VERSION).tar.xz:
	$(call download,$(GNUTLS_URL))

.sum-gnutls: gnutls-$(GNUTLS_VERSION).tar.xz

gnutls: gnutls-$(GNUTLS_VERSION).tar.xz .sum-gnutls
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/gnutls/gnutls-win32.patch
else
	$(APPLY) $(SRC)/gnutls/downgrade-gettext-requirement.patch
endif
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/gnutls/no-create-time-h.patch
endif
#ifdef HAVE_MACOSX
#	$(APPLY) $(SRC)/gnutls/gnutls-pkgconfig-osx.patch
#endif
	$(APPLY) $(SRC)/gnutls/gnutls-no-egd.patch
	$(APPLY) $(SRC)/gnutls/read-file-limits.h.patch
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
	--disable-psk-authentication-FIXME \
	--with-included-libtasn1 \
	--disable-openpgp-authentication \
	--disable-openssl-compatibility \
	--disable-guile \
	--disable-nls \
	--disable-dtls-srtp-support \
	--without-libintl-prefix \
	--disable-non-suiteb-curves \
	$(HOSTCONF)

ifdef HAVE_ANDROID
	GNUTLS_CONF += --disable-hardware-acceleration
endif

DEPS_gnutls = nettle $(DEPS_nettle) iconv $(DEPS_iconv)


#Workaround for localtime_r function
ifdef HAVE_WIN32
CFLAGS="-D_POSIX_C_SOURCE"
endif

.gnutls: gnutls
	$(RECONF)
ifdef HAVE_ANDROID
	cd $< && $(HOSTVARS) gl_cv_header_working_stdint_h=yes ./configure $(GNUTLS_CONF)
else
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS)" ./configure $(GNUTLS_CONF)
endif
	cd $</gl && $(MAKE) install
	cd $</lib && $(MAKE) install
	touch $@
