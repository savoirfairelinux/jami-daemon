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
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/upnp/libupnp-win32.patch
	$(APPLY) $(SRC)/upnp/libupnp-win64.patch
	$(APPLY) $(SRC)/upnp/threadpool.patch
endif
ifdef HAVE_WIN64
	$(APPLY) $(SRC)/upnp/win_inet_pton.patch
endif
	$(APPLY) $(SRC)/upnp/libupnp-ipv6.patch
	$(APPLY) $(SRC)/upnp/miniserver.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub
	$(MOVE)

.upnp: upnp
ifdef HAVE_WIN32
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="-DUPNP_STATIC_LIB" ./configure --disable-samples --without-documentation --disable-blocking_tcp_connections $(HOSTCONF)
else
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DUPNP_STATIC_LIB" ./configure --disable-samples --without-documentation --disable-blocking_tcp_connections $(HOSTCONF)
endif
	cd $< && $(MAKE) install
	touch $@
