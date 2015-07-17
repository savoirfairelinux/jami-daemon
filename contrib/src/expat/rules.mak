# EXPAT

EXPAT_VERSION := 2.1.0
EXPAT_URL := $(SF)/expat/expat-$(EXPAT_VERSION).tar.gz
EXPAT_GITURL := https://github.com/coapp-packages/expat.git
EXPAT_HASH := 5ccfa1602907957bf11dc3a1a1cfd683ab3758ae

PKGS += expat
ifeq ($(call need_pkg,"expat >= 1.95.0"),)
PKGS_FOUND += expat
endif

$(TARBALLS)/expat-$(EXPAT_HASH).tar.xz:
	$(call download_git,$(EXPAT_GITURL), master, $(EXPAT_HASH))

.sum-expat: expat-$(EXPAT_HASH).tar.xz
	$(warning Not implemented.)
	touch $@

expat: expat-$(EXPAT_HASH).tar.xz .sum-expat
	rm -Rf $@ $@-$(EXPAT_HASH)
	mkdir -p $@-$(EXPAT_HASH)
	(cd $@-$(EXPAT_HASH) && tar xv --strip-components=1 -f ../$<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.expat: expat
	cd $< && $(HOSTVARS) bash ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
