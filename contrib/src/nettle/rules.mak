# Nettle

NETTLE_VERSION := 91df68136ad1562cf9217599254706d8cfa970ea
NETTLE_URL := https://git.lysator.liu.se/aberaud/nettle/-/archive/$(NETTLE_VERSION)/nettle-$(NETTLE_VERSION).tar.gz
PKGS += nettle

ifeq ($(call need_pkg,"nettle >= 3.4.1"),)
PKGS_FOUND += nettle
endif

$(TARBALLS)/nettle-$(NETTLE_VERSION).tar.gz:
	$(call download,$(NETTLE_URL))

.sum-nettle: nettle-$(NETTLE_VERSION).tar.gz

nettle: nettle-$(NETTLE_VERSION).tar.gz .sum-nettle
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_nettle = gmp

.nettle: nettle
ifdef HAVE_IOS
	cd $< && sed -i.orig s/-ggdb3//g configure.ac
endif
	cd $< && autoreconf
	cd $< && $(HOSTVARS) ./configure --disable-documentation $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
