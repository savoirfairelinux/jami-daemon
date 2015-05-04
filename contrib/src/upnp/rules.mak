# UPNP
UPNP_VERSION := 1.6.19
UPNP_URL := http://sourceforge.net/projects/pupnp/files/pupnp/libUPnP%20$(UPNP_VERSION)/libupnp-$(UPNP_VERSION).tar.bz2/download

PKGS += upnp
ifeq ($(call need_pkg,'libupnp'),)
PKGS_FOUND += upnp
endif

$(TARBALLS)/libupnp-$(UPNP_VERSION).tar.bz2:
	$(call download,$(UPNP_URL))

.sum-upnp: libupnp-$(UPNP_VERSION).tar.bz2

upnp: libupnp-$(UPNP_VERSION).tar.bz2 .sum-upnp
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/upnp/libupnp-win32.patch
	$(APPLY) $(SRC)/upnp/libupnp-win64.patch
	$(APPLY) $(SRC)/upnp/threadpool.patch
endif
	$(APPLY) $(SRC)/upnp/libupnp-ipv6.patch
	$(APPLY) $(SRC)/upnp/miniserver.patch
	$(APPLY) $(SRC)/upnp/uuid_upnp.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub build-aux/
	$(MOVE)

.upnp: upnp
ifdef HAVE_WIN32
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="-DUPNP_STATIC_LIB" ./configure --disable-samples --without-documentation --disable-blocking_tcp_connections $(HOSTCONF)
else
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DUPNP_STATIC_LIB" ./configure --disable-samples --without-documentation --disable-blocking_tcp_connections $(HOSTCONF)
endif
	cd $< && $(MAKE) install
	touch $@
