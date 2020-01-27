# OPENDHT
GIT_URL         := git://github.com/257/opendht.git
GIT_BRANCH      := connstat
GIT_COMMIT      := 17304f11920cf583132f2cc5cb070c8c5f061f0a

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

OPENDHT_CONF_FLAGS += $(HOSTCONF)
OPENDHT_CONF_FLAGS += --enable-static
OPENDHT_CONF_FLAGS += --disable-shared
OPENDHT_CONF_FLAGS += --disable-tools
OPENDHT_CONF_FLAGS += --disable-indexation
OPENDHT_CONF_FLAGS += --disable-python
OPENDHT_CONF_FLAGS += --disable-doc
OPENDHT_CONF_FLAGS += --enable-proxy-server
OPENDHT_CONF_FLAGS += --enable-proxy-client
OPENDHT_CONF_FLAGS += --with-http-parser-fork
OPENDHT_CONF_FLAGS += --enable-push-notifications
ifdef ENABLE_CONNSTAT
    OPENDHT_CONF_FLAGS += --enable-connstat
endif
# fmt 5.3.0 fix: https://github.com/fmtlib/fmt/issues/1267
OPENDHT_CONF = FMT_USE_USER_DEFINED_LITERALS=0

$(TARBALLS)/opendht-$(OPENDHT_VERSION).tar.gz:
	$(call download,$(OPENDHT_URL))

$(GITREPOS)/opendht.git:
	$(call download_git2,$(GIT_URL),$(GIT_BRANCH))

.sum-opendht: opendht
	$(call gitverify,$<,$(GIT_COMMIT))
	touch $@

opendht: $(GITREPOS)/opendht.git
	$(UPDATE_AUTOCONFIG)
	$(MOVE_GIT)

.opendht: opendht .sum-opendht
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) $(OPENDHT_CONF) ./configure $(OPENDHT_CONF_FLAGS)
	cd $< && $(MAKE) install
	touch $@
