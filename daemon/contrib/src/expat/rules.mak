# EXPAT

EXPAT_VERSION := 2.1.0
EXPAT_URL := $(SF)/expat/expat-$(EXPAT_VERSION).tar.gz

PKGS += expat
ifeq ($(call need_pkg,"expat >= 1.95.0"),)
PKGS_FOUND += expat
endif

$(TARBALLS)/expat-$(EXPAT_VERSION).tar.gz:
	$(call download,$(EXPAT_URL))

.sum-expat: expat-$(EXPAT_VERSION).tar.gz

expat: expat-$(EXPAT_VERSION).tar.gz .sum-expat
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.expat: expat
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
