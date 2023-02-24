# Nettle

NETTLE_VERSION := nettle_3.8_release_20220602
NETTLE_URL := https://git.lysator.liu.se/nettle/nettle/-/archive/$(NETTLE_VERSION)/nettle-$(NETTLE_VERSION).tar.gz
PKGS += nettle

ifeq ($(call need_pkg,"nettle >= 3.6"),)
PKGS_FOUND += nettle
endif
DEPS_nettle = gmp

$(TARBALLS)/nettle-$(NETTLE_VERSION).tar.gz:
	$(call download,$(NETTLE_URL))

.sum-nettle: nettle-$(NETTLE_VERSION).tar.gz

nettle: nettle-$(NETTLE_VERSION).tar.gz .sum-nettle
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)



.nettle: nettle
ifdef HAVE_IOS
	cd $< && sed -i.orig s/-ggdb3//g configure.ac
endif
	cd $< && autoreconf
	cd $< && $(HOSTVARS) ./configure --disable-documentation $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
