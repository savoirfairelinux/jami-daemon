# UPNP
UPNP_VERSION := 1.8.4
UPNP_URL := https://github.com/mrjimenez/pupnp/archive/release-$(UPNP_VERSION).tar.gz

PKGS += upnp
ifeq ($(call need_pkg,"libupnp >= 1.8.4"),)
PKGS_FOUND += upnp
endif

$(TARBALLS)/pupnp-release-$(UPNP_VERSION).tar.gz:
	$(call download,$(UPNP_URL))

.sum-upnp: pupnp-release-$(UPNP_VERSION).tar.gz

upnp: pupnp-release-$(UPNP_VERSION).tar.gz .sum-upnp
	$(UNPACK)
if defined(HAVE_WIN32) || defined(HAVE_WIN64)
	$(APPLY) $(SRC)/upnp/libupnp-windows.patch
endif
	$(APPLY) $(SRC)/upnp/libupnp-ipv6.patch
	$(APPLY) $(SRC)/upnp/miniserver.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub
	$(MOVE)

.upnp: upnp
ifdef HAVE_WIN32
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="-DUPNP_STATIC_LIB" ./configure --disable-largefile --disable-samples --without-documentation --disable-blocking_tcp_connections $(HOSTCONF)
else
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DUPNP_STATIC_LIB" ./configure --disable-largefile --disable-samples --without-documentation --disable-blocking_tcp_connections $(HOSTCONF)
endif
	cd $< && $(MAKE) install
	touch $@
