# DHT
DHT_VERSION := adaf1e77d34d1edbf50f27c96a70839ff7f2da25
DHT_URL := https://github.com/aberaud/dht/archive/$(DHT_VERSION).tar.gz

PKGS += dht
ifeq ($(call need_pkg,'opendht'),)
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
