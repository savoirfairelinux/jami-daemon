# GnuTLS

GNUTLS_VERSION := 3.6.6
GNUTLS_URL := https://www.gnupg.org/ftp/gcrypt/gnutls/v3.6/gnutls-$(GNUTLS_VERSION).tar.xz

PKGS += gnutls

ifeq ($(call need_pkg,"gnutls >= 3.5.17"),)
PKGS_FOUND += gnutls
endif

$(TARBALLS)/gnutls-$(GNUTLS_VERSION).tar.xz:
	$(call download,$(GNUTLS_URL))

.sum-gnutls: gnutls-$(GNUTLS_VERSION).tar.xz

gnutls: gnutls-$(GNUTLS_VERSION).tar.xz .sum-gnutls
	$(UNPACK)
	$(APPLY) $(SRC)/gnutls/gtk-doc.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/gnutls/gnutls-win32.patch
else
	$(APPLY) $(SRC)/gnutls/downgrade-gettext-requirement.patch
endif
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/gnutls/no-create-time-h.patch
endif
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/gnutls/gnutls-disable-getentropy-osx.patch
endif
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
	--with-included-unistring \
	--disable-openpgp-authentication \
	--disable-openssl-compatibility \
	--disable-guile \
	--disable-nls \
	--disable-dtls-srtp-support \
	--without-libintl-prefix \
	--without-idn \
	$(HOSTCONF)

ifdef HAVE_ANDROID
	GNUTLS_CONF += --disable-hardware-acceleration
endif

ifdef HAVE_IOS
	GNUTLS_CONF += --disable-hardware-acceleration
endif

DEPS_gnutls = nettle iconv


#Workaround for localtime_r function
ifdef HAVE_WIN32
CFLAGS="-D_POSIX_C_SOURCE"
endif

.gnutls: gnutls
	$(RECONF)
ifdef HAVE_ANDROID
	cd $< && $(HOSTVARS) gl_cv_header_working_stdint_h=yes ./configure $(GNUTLS_CONF)
else
ifdef HAVE_IOS
	cd $< && $(HOSTVARS) ac_cv_func_clock_gettime=no CFLAGS="$(CFLAGS)" ./configure $(GNUTLS_CONF)
else
ifdef HAVE_MACOSX
	cd $< && $(HOSTVARS) ac_cv_func_clock_gettime=no CFLAGS="$(CFLAGS)" ./configure $(GNUTLS_CONF)
else
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS)" ./configure $(GNUTLS_CONF)
endif
endif
endif
	cd $</gl && $(MAKE) install
	cd $</lib && $(MAKE) install
	touch $@
