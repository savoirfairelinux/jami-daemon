# OPENDHT
OPENDHT_VERSION := 3.7.2
OPENDHT_URL := https://github.com/savoirfairelinux/opendht/archive/v$(OPENDHT_VERSION).tar.gz

PKGS += opendht
ifeq ($(call need_pkg,'opendht >= 3.6.0'),)
PKGS_FOUND += opendht
endif

DEPS_opendht += msgpack argon2 libressl restinio jsoncpp gnutls asio fmt simdutf

OPENDHT_CONF = -DCMAKE_CXX_STANDARD=20 \
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

CMAKE_PKGS += opendht
