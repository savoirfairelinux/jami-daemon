# DHTNET
DHTNET_VERSION := 1f1e277ef4af292d660e5c2e45be8ce5a9f3ef49
DHTNET_URL := https://git.jami.net/savoirfairelinux/dhtnet/-/archive/$(DHTNET_VERSION)/dhtnet-$(DHTNET_VERSION).tar.gz

PKGS += dhtnet
DEPS_dhtnet += opendht pjproject asio upnp fmt

ifndef HAVE_WIN32
DEPS_dhtnet += natpmp
endif

DHTNET_CONF = -DCMAKE_CXX_STANDARD=20 \
	-DBUILD_BENCHMARKS=Off \
	-DBUILD_TOOLS=Off \
	-DBUILD_TESTING=Off \
	-DBUILD_DEPENDENCIES=Off \
	-DBUILD_EXAMPLE=Off

$(TARBALLS)/dhtnet-$(DHTNET_VERSION).tar.gz:
	$(call download,$(DHTNET_URL))

.sum-dhtnet: dhtnet-$(DHTNET_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

dhtnet: dhtnet-$(DHTNET_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) $(SRC)/dhtnet/0001-interrupt-DTLS-handshake-on-shutdown.patch
	$(MOVE)

CMAKE_PKGS += dhtnet
