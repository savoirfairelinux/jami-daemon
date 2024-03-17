# libiconv
LIBICONV_VERSION=1.17
LIBICONV_URL=$(GNU)/libiconv/libiconv-$(LIBICONV_VERSION).tar.gz

PKGS += iconv
# iconv cannot be detect with pkg-config, but it is mandated by POSIX.
# Hard-code based on the operating system.
ifndef HAVE_WIN32
ifndef HAVE_ANDROID
PKGS_FOUND += iconv
endif
endif

$(TARBALLS)/libiconv-$(LIBICONV_VERSION).tar.gz:
	$(call download,$(LIBICONV_URL))

.sum-iconv: libiconv-$(LIBICONV_VERSION).tar.gz

iconv: libiconv-$(LIBICONV_VERSION).tar.gz .sum-iconv
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub build-aux
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub libcharset/build-aux
	$(MOVE)

.iconv: iconv
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-nls
	cd $< && $(MAKE) install
	touch $@
