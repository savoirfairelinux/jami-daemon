# DHT
DHT_VERSION := 56cb6067140ea9ccc85ee880037ad25c13adae1a
DHT_URL := https://github.com/aberaud/dht/archive/$(DHT_VERSION).tar.gz

PKGS += dht
ifeq ($(call need_pkg,'opendht'),)
PKGS_FOUND += dht
endif

# Avoid building distro-provided dependencies in case dht was built manually
ifneq ($(call need_pkg,"gnutls >= 3.0.20"),)
DEPS_dht = gnutls $(DEPS_gnutls)
endif

$(TARBALLS)/dht-$(DHT_VERSION).tar.gz:
	$(call download,$(DHT_URL))

.sum-dht: dht-$(DHT_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

dht: dht-$(DHT_VERSION).tar.gz .sum-dht
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.dht: dht
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
