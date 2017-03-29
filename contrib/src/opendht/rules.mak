# OPENDHT
OPENDHT_HASH := c424eb91609f6731261a9c6c466c9862a1f1b839
OPENDHT_GITURL := https://github.com/savoirfairelinux/opendht.git

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

$(TARBALLS)/opendht-$(OPENDHT_HASH).tar.xz:
	$(call download_git,$(OPENDHT_GITURL),master,$(OPENDHT_HASH),$(GIT) submodule update --init)

.sum-opendht: opendht-$(OPENDHT_HASH).tar.xz
	$(warning $@ not implemented)
	touch $@

opendht: opendht-$(OPENDHT_HASH).tar.xz .sum-opendht
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.opendht: opendht
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure --disable-tools --disable-python --disable-doc $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
