# OPENDHT
OPENDHT_VERSION := 0.6.3
OPENDHT_URL := https://github.com/savoirfairelinux/opendht/archive/$(OPENDHT_VERSION).tar.gz

PKGS += opendht
ifeq ($(call need_pkg,'opendht'),)
PKGS_FOUND += opendht
endif

# Avoid building distro-provided dependencies in case opendht was built manually
ifneq ($(call need_pkg,"msgpack >= 1.2"),)
DEPS_opendht += msgpack
endif
ifneq ($(call need_pkg,"gnutls >= 3.3.0"),)
DEPS_opendht += gnutls $(DEPS_gnutls)
endif

$(TARBALLS)/opendht-$(OPENDHT_VERSION).tar.gz:
	$(call download,$(OPENDHT_URL))

.sum-opendht: opendht-$(OPENDHT_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

opendht: opendht-$(OPENDHT_VERSION).tar.gz .sum-opendht
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.opendht: opendht
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure --disable-tools --disable-python --disable-doc $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
