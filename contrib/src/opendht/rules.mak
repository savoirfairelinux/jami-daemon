# OPENDHT
# TODO BUMP OPENDHT FOR PROXY ABILITIES
OPENDHT_VERSION := d2968772e3a114ecbd8a96b087d4e2ed87cdbc19
OPENDHT_URL := https://github.com/AmarOk1412/opendht/archive/$(OPENDHT_VERSION).tar.gz

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
DEPS_opendht += gnutls $(DEPS_gnutls)
endif

$(TARBALLS)/opendht-$(OPENDHT_VERSION).tar.gz:
	$(call download,$(OPENDHT_URL))

.sum-opendht: opendht-$(OPENDHT_VERSION).tar.gz

opendht: opendht-$(OPENDHT_VERSION).tar.gz
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.opendht: opendht .sum-opendht
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure --disable-tools --disable-python --disable-doc --enable-proxy-server --enable-proxy-client --enable-push-notifications $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
