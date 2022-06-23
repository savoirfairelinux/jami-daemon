# OPENDHT
OPENDHT_VERSION := 0183c2191edd52e1e32d6dfe29169ad721ff3a57
OPENDHT_URL := https://github.com/savoirfairelinux/opendht/archive/$(OPENDHT_VERSION).tar.gz

PKGS += opendht
ifeq ($(call need_pkg,'opendht >= 2.3.2'),)
PKGS_FOUND += opendht
endif

# Avoid building distro-provided dependencies in case opendht was built manually
ifneq ($(call need_pkg,"msgpack >= 1.2"),)
DEPS_opendht += msgpack
endif
ifneq ($(call need_pkg,"libargon2"),)
DEPS_opendht += argon2
endif
ifneq ($(and $(call need_pkg,"openssl >= 1.1.0"),$(call need_pkg,"libressl >= 1.12.2")),)
DEPS_opendht += libressl
endif
ifneq ($(call need_pkg,"restinio >= v.0.5.1"),)
DEPS_opendht += restinio
endif
ifneq ($(call need_pkg,"jsoncpp >= 1.7.2"),)
DEPS_opendht += jsoncpp
endif
ifneq ($(call need_pkg,"gnutls >= 3.3.0"),)
DEPS_opendht += gnutls
endif

# fmt 5.3.0 fix: https://github.com/fmtlib/fmt/issues/1267
OPENDHT_CONF = FMT_USE_USER_DEFINED_LITERALS=0

OPENDHT_CMAKECONF = -DOPENDHT_STATIC=On \
					-DOPENDHT_SHARED=Off \
					-DOPENDHT_TOOLS=Off \
					-DOPENDHT_INDEXATION=Off \
					-DOPENDHT_PYTHON=Off \
					-DOPENDHT_PROXY_SERVER=On \
					-DOPENDHT_PROXY_CLIENT=On \
					-DOPENDHT_PUSH_NOTIFICATIONs=On

$(TARBALLS)/opendht-$(OPENDHT_VERSION).tar.gz:
	$(call download,$(OPENDHT_URL))

.sum-opendht: opendht-$(OPENDHT_VERSION).tar.gz

opendht: opendht-$(OPENDHT_VERSION).tar.gz
	$(UNPACK)
	cd $(UNPACK_DIR)
	$(MOVE)

.opendht: opendht .sum-opendht
	cd $< && $(HOSTVARS) $(OPENDHT_CONF) $(CMAKE) $(OPENDHT_CMAKECONF) .
	cd $< && $(MAKE) install
	touch $@
