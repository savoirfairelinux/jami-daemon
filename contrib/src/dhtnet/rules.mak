# DHTNET
DHTNET_VERSION := c541f16426aae71121b3f8c2d29e41e8954e6218
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
	$(MOVE)

CMAKE_PKGS += dhtnet
