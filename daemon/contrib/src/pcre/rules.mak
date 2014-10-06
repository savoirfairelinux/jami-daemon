# Perl Compatible Regular Expression

PCRE_VERSION := 8.35
PCRE_URL := ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre-$(PCRE_VERSION).tar.bz2

PKGS += pcre

# OS X ships with improperly packaged libpcre, so we can't rely on pkg-config
ifndef HAVE_MACOSX
ifeq ($(call need_pkg,"libpcre"),)
PKGS_FOUND += pcre
endif
endif

$(TARBALLS)/pcre-$(PCRE_VERSION).tar.bz2:
	$(call download,$(PCRE_URL))

.sum-pcre: pcre-$(PCRE_VERSION).tar.bz2

pcre: pcre-$(PCRE_VERSION).tar.bz2 .sum-pcre
	$(UNPACK)
	$(MOVE)

.pcre: pcre
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
