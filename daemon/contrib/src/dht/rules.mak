# DHT
DHT_VERSION := a7c6df3092b0092a149acb8d9a9f6a78349bc0f5
DHT_URL := https://github.com/aberaud/dht/archive/$(DHT_VERSION).tar.gz

PKGS += dht
ifeq ($(call need_pkg,'dhtcpp'),)
PKGS_FOUND += dht
endif

DEPS_dht = gnutls

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
