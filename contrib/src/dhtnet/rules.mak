# DHTNET
DHTNET_VERSION := d61f7946f3bb2dc10f3f0dca5e3f8e033fc5acba
DHTNET_URL := https://git.jami.net/savoirfairelinux/dhtnet/-/archive/$(DHTNET_VERSION)/dhtnet-$(DHTNET_VERSION).tar.gz

PKGS += dhtnet
DEPS_dhtnet += opendht pjproject asio upnp fmt

ifndef HAVE_WIN32
DEPS_dhtnet += natpmp
endif


DHTNET_CONF = -DBUILD_SHARED_LIBS=Off \
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

.dhtnet: dhtnet toolchain.cmake .sum-dhtnet
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) $(DHTNET_CONF) ..
	cd $< && cd build && $(MAKE) install
	touch $@
