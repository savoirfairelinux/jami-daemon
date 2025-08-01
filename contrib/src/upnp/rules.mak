# UPNP
UPNP_VERSION := 1.14.18
PKG_CPE += cpe:2.3:a:pupnp_project:pupnp:$(UPNP_VERSION):*:*:*:*:*:*:*
UPNP_URL := https://github.com/pupnp/pupnp/archive/release-$(UPNP_VERSION).tar.gz

PKGS += upnp
ifeq ($(call need_pkg,"libupnp >= 1.14.18"),)
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
	$(APPLY) $(SRC)/upnp/poll.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv -f config.guess config.sub
	$(MOVE)

PUPNP_OPTIONS=--disable-blocking_tcp_connections --disable-largefile --disable-samples --disable-device --disable-webserver --without-documentation

.upnp: upnp
ifdef HAVE_WIN32
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="-DUPNP_STATIC_LIB" ./configure $(PUPNP_OPTIONS) $(HOSTCONF)
else
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DUPNP_STATIC_LIB" ./configure $(PUPNP_OPTIONS) $(HOSTCONF)
endif
	cd $< && $(MAKE) install
	touch $@
