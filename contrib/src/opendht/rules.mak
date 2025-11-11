# OPENDHT
OPENDHT_VERSION := 3b4139273b36d9b503808ca54ca7a2cf568cf698
OPENDHT_URL := https://github.com/savoirfairelinux/opendht/archive/$(OPENDHT_VERSION).tar.gz

PKGS += opendht
ifeq ($(call need_pkg,'opendht >= 3.3.0'),)
PKGS_FOUND += opendht
endif

DEPS_opendht += msgpack argon2 libressl restinio jsoncpp gnutls asio fmt simdutf

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

.opendht: opendht toolchain.cmake .sum-opendht
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) $(OPENDHT_CONF) ..
	cd $< && cd build && $(MAKE) install
	touch $@
