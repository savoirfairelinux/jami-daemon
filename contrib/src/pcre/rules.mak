# Perl Compatible Regular Expression

PCRE_VERSION := 8.41
PCRE_URL := https://ftp.pcre.org/pub/pcre/pcre-$(PCRE_VERSION).tar.bz2

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
	$(APPLY) $(SRC)/pcre/0001-build-don-t-build-executables.patch
	$(MOVE)

.pcre: pcre
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure --disable-cpp $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
