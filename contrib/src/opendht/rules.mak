# OPENDHT
OPENDHT_VERSION := 87f6f3df8b28d14ff383f2da448b1b2ce1248464
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
ifneq ($(call need_pkg,"libressl >= 1-12-2"),)
DEPS_opendht += libressl	
endif
ifneq ($(call need_pkg,"restinio >= v.0.5.1"),)
DEPS_opendht += restinio
endif
ifneq ($(call need_pkg,"jsoncpp"),)
DEPS_opendht += jsoncpp
endif
ifneq ($(call need_pkg,"gnutls >= 3.3.0"),)
DEPS_opendht += gnutls
endif

# fmt 5.3.0 fix: https://github.com/fmtlib/fmt/issues/1267
OPENDHT_CONF = FMT_USE_USER_DEFINED_LITERALS=0

$(TARBALLS)/opendht-$(OPENDHT_VERSION).tar.gz:
	$(call download,$(OPENDHT_URL))

.sum-opendht: opendht-$(OPENDHT_VERSION).tar.gz

opendht: opendht-$(OPENDHT_VERSION).tar.gz
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.opendht: opendht .sum-opendht
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) $(OPENDHT_CONF) ./configure --enable-static --disable-shared --disable-tools --disable-python --disable-doc --enable-proxy-server --enable-proxy-client --with-http-parser-fork --enable-push-notifications $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
