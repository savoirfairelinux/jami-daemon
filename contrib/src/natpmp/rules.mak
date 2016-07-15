# libnatpmp
NATPMP_VERSION := 20150609
NATPMP_URL := http://miniupnp.free.fr/files/download.php?file=libnatpmp-$(NATPMP_VERSION).tar.gz

PKGS += natpmp
ifeq ($(call need_pkg,'libnatpmp'),)
PKGS_FOUND += natpmp
endif

$(TARBALLS)/libnatpmp-$(NATPMP_VERSION).tar.gz:
	$(call download,$(NATPMP_URL))

.sum-natpmp: libnatpmp-$(NATPMP_VERSION).tar.gz

natpmp: libnatpmp-$(NATPMP_VERSION).tar.gz .sum-natpmp
	$(UNPACK)
	$(MOVE)

.natpmp: natpmp
	cd $< && $(MAKE) INSTALLPREFIX="$(PREFIX)" install
	touch $@
