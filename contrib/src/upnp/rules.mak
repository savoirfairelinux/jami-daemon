# UPNP
UPNP_VERSION := 1.14.7
UPNP_URL := https://github.com/pupnp/pupnp/archive/release-$(UPNP_VERSION).tar.gz

PKGS += upnp
ifeq ($(call need_pkg,"libupnp >= 1.8.4"),)
PKGS_FOUND += upnp
endif

$(TARBALLS)/pupnp-release-$(UPNP_VERSION).tar.gz:
	$(call download,$(UPNP_URL))

.sum-upnp: pupnp-release-$(UPNP_VERSION).tar.gz

upnp: pupnp-release-$(UPNP_VERSION).tar.gz .sum-upnp
	$(UNPACK)
ifeq ($(OS),Windows_NT)
	$(APPLY) $(SRC)/upnp/libupnp-windows.patch
endif
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub
	$(MOVE)

.upnp: upnp
ifdef HAVE_WIN32
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="-DUPNP_STATIC_LIB" ./configure --disable-largefile --disable-samples --disable-device --disable-webserver --without-documentation $(HOSTCONF)
else
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DUPNP_STATIC_LIB" ./configure --disable-largefile --disable-samples --disable-device --disable-webserver --without-documentation $(HOSTCONF)
endif
	cd $< && $(MAKE) install
	touch $@
