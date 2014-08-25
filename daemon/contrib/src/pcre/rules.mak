# Perl Compatible Regular Expression

PCRE_VERSION := 8.35
PCRE_URL := ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre-$(PCRE_VERSION).tar.bz2

PKGS += pcre
ifeq ($(call need_pkg,"pcre >= 8.33"),)
PKGS_FOUND += pcre
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
