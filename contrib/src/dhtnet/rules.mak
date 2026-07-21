# DHTNET
DHTNET_VERSION := 4c2210f47c2adc2aef726cce30a6d322f0855ad4
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
	$(APPLY) $(SRC)/dhtnet/0001-avoid-pupnp-self-join-on-discovery-result.patch
	$(MOVE)

CMAKE_PKGS += dhtnet
