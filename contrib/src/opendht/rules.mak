# OPENDHT
OPENDHT_VERSION := 5237f0a3b3eb8965f294de706ad73596569ae1dd
OPENDHT_URL := https://github.com/savoirfairelinux/opendht/archive/$(OPENDHT_VERSION).tar.gz

PKGS += opendht
ifeq ($(call need_pkg,'opendht >= 3.1.7'),)
PKGS_FOUND += opendht
endif

# Avoid building distro-provided dependencies in case opendht was built manually
DEPS_opendht += msgpack argon2 libressl restinio jsoncpp gnutls asio

OPENDHT_CONF = -DBUILD_SHARED_LIBS=Off \
	-DBUILD_TESTING=Off \
	-DOPENDHT_DOCUMENTATION=Off \
	-DOPENDHT_PROXY_CLIENT=On \
	-DOPENDHT_PROXY_SERVER=On \
	-DOPENDHT_PUSH_NOTIFICATIONS=On \
	-DOPENDHT_IO_URING=Off \
	-DOPENDHT_TOOLS=Off

$(TARBALLS)/opendht-$(OPENDHT_VERSION).tar.gz:
	$(call download,$(OPENDHT_URL))

.sum-opendht: opendht-$(OPENDHT_VERSION).tar.gz

opendht: opendht-$(OPENDHT_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.opendht: opendht .sum-opendht
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) $(OPENDHT_CONF) ..
	cd $< && cd build && $(MAKE) install
	touch $@
