# DHTNET
DHTNET_VERSION := 3bd76bcc786d9db2b9efb118f625092146d3591b
DHTNET_URL := https://review.jami.net/plugins/gitiles/dhtnet/+archive/$(DHTNET_VERSION).tar.gz

PKGS += dhtnet
DEPS_dhtnet += opendht pjproject asio upnp

ifndef HAVE_WIN32
ifndef HAVE_ANDROID
ifndef HAVE_IOS
ifndef HAVE_MACOSX
DEPS_dhtnet += natpmp
endif
endif
endif
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
	mkdir -p $(UNPACK_DIR)
	$(UNPACK) -C $(UNPACK_DIR)
	$(MOVE)

.dhtnet: dhtnet toolchain.cmake .sum-dhtnet
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) $(DHTNET_CONF) ..
	cd $< && cd build && $(MAKE) install
	touch $@
