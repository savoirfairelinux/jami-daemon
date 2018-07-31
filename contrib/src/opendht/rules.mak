# OPENDHT
OPENDHT_VERSION := 1.7.4
OPENDHT_URL := https://github.com/savoirfairelinux/opendht/archive/$(OPENDHT_VERSION).tar.gz

PKGS += opendht
ifeq ($(call need_pkg,'opendht'),)
PKGS_FOUND += opendht
endif

# Avoid building distro-provided dependencies in case opendht was built manually
ifneq ($(call need_pkg,"msgpack >= 1.2"),)
DEPS_opendht += msgpack
endif
ifneq ($(call need_pkg,"libargon2"),)
DEPS_opendht += argon2
endif
ifneq ($(call need_pkg,"restbed"),)
DEPS_opendht += restbed
endif
ifneq ($(call need_pkg,"jsoncpp"),)
DEPS_opendht += jsoncpp
endif
ifneq ($(call need_pkg,"gnutls >= 3.3.0"),)
DEPS_opendht += gnutls
endif

$(TARBALLS)/opendht-$(OPENDHT_VERSION).tar.gz:
	$(call download,$(OPENDHT_URL))

.sum-opendht: opendht-$(OPENDHT_VERSION).tar.gz

opendht: opendht-$(OPENDHT_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.opendht: opendht .sum-opendht
	cd $< && $(HOSTVARS) $(CMAKE) -DOPENDHT_STATIC=On -DOPENDHT_SHARED=Off -DOPENDHT_TOOLS=Off -DOPENDHT_PROXY_CLIENT=On -DOPENDHT_PUSH_NOTIFICATIONS=On -DOPENDHT_DOCUMENTATION=Off .
	cd $< && $(MAKE) install
	touch $@
