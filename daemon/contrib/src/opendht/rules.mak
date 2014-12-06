# OPENDHT
OPENDHT_VERSION := 79ef324cd35552ed37ea221c63d35291fb0308c2
OPENDHT_URL := https://github.com/aberaud/dht/archive/$(OPENDHT_VERSION).tar.gz

PKGS += opendht
ifeq ($(call need_pkg,'opendht'),)
PKGS_FOUND += opendht
endif

# Avoid building distro-provided dependencies in case opendht was built manually
ifneq ($(call need_pkg,"gnutls >= 3.0.20"),)
DEPS_opendht = gnutls $(DEPS_gnutls)
endif

$(TARBALLS)/opendht-$(OPENDHT_VERSION).tar.gz:
	$(call download,$(OPENDHT_URL))

.sum-opendht: opendht-$(OPENDHT_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

opendht: opendht-$(OPENDHT_VERSION).tar.gz .sum-opendht
	$(UNPACK)
	mv dht-$(OPENDHT_VERSION) $(UNPACK_DIR)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.opendht: opendht
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
