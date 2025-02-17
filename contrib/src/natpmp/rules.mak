# libnatpmp
NATPMP_VERSION := 007c3a53165a0551c877130eea4d966885ce19ae
NATPMP_URL := https://github.com/miniupnp/libnatpmp/archive/${NATPMP_VERSION}.tar.gz

ifndef HAVE_WIN32
ifndef HAVE_IOS
PKGS += natpmp
endif
endif

ifeq ($(call need_pkg,'libnatpmp'),)
PKGS_FOUND += natpmp
endif

$(TARBALLS)/libnatpmp-$(NATPMP_VERSION).tar.gz:
	$(call download,$(NATPMP_URL))

.sum-natpmp: libnatpmp-$(NATPMP_VERSION).tar.gz

natpmp: libnatpmp-$(NATPMP_VERSION).tar.gz .sum-natpmp
	$(UNPACK)
	$(MOVE)

.natpmp: natpmp toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) .
	cd $< && $(MAKE) install
	cd $< && cp natpmp_declspec.h $(PREFIX)/include/
	touch $@
