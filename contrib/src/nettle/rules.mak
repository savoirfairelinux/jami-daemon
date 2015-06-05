# Nettle

NETTLE_VERSION := 3.1
NETTLE_URL := $(GNU)/nettle/nettle-$(NETTLE_VERSION).tar.gz

ifeq ($(call need_pkg,"nettle >= 3.1"),)
PKGS_FOUND += nettle
endif

$(TARBALLS)/nettle-$(NETTLE_VERSION).tar.gz:
	$(call download,$(NETTLE_URL))

.sum-nettle: nettle-$(NETTLE_VERSION).tar.gz

nettle: nettle-$(NETTLE_VERSION).tar.gz .sum-nettle
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_nettle = gmp $(DEPS_gmp)

.nettle: nettle
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
