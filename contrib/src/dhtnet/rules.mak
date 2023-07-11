# DHTNET
DHTNET_VERSION := d50178b4a076b0dbd79dd1176c8182276d38b14d
DHTNET_URL := https://review.jami.net/plugins/gitiles/dhtnet/+archive/$(DHTNET_VERSION).tar.gz

PKGS += dhtnet

DEPS_dhtnet += opendht pjproject asio

DHTNET_CONF = -DBUILD_SHARED_LIBS=Off \
	-DBUILD_TESTING=Off

$(TARBALLS)/dhtnet-$(DHTNET_VERSION).tar.gz:
	$(call download,$(DHTNET_URL))

.sum-dhtnet: dhtnet-$(DHTNET_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

dhtnet: dhtnet-$(DHTNET_VERSION).tar.gz
	mkdir -p $(UNPACK_DIR)
	$(UNPACK) -C $(UNPACK_DIR)
	$(MOVE)

.dhtnet: dhtnet .sum-dhtnet
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) $(DHTNET_CONF) ..
	cd $< && cd build && $(MAKE) install
	touch $@
