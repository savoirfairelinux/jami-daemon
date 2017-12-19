# Nettle

NETTLE_VERSION := 3.3
NETTLE_URL := $(GNU)/nettle/nettle-$(NETTLE_VERSION).tar.gz

PKGS += nettle

ifeq ($(call need_pkg,"nettle >= 3.3"),)
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
	cd $< && autoreconf
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -O3" ./configure $(HOSTCONF)
else
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
endif
	cd $< && $(MAKE) install
	touch $@
