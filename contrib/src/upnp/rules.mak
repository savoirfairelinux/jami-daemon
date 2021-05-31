# UPNP
UPNP_VERSION := cfbb7910445df185ca34601eabb039c12ec5474c
UPNP_URL := https://github.com/pupnp/pupnp/archive/$(UPNP_VERSION).tar.gz

PKGS += upnp
ifeq ($(call need_pkg,"libupnp >= 1.8.4"),)
PKGS_FOUND += upnp
endif

$(TARBALLS)/pupnp-$(UPNP_VERSION).tar.gz:
	$(call download,$(UPNP_URL))

.sum-upnp: pupnp-$(UPNP_VERSION).tar.gz

upnp: pupnp-$(UPNP_VERSION).tar.gz .sum-upnp
	$(UNPACK)
ifeq ($(OS),Windows_NT)
	$(APPLY) $(SRC)/upnp/libupnp-windows.patch
endif
	$(APPLY) $(SRC)/upnp/debug.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub
	$(MOVE)

PUPNP_OPTIONS=--disable-largefile --disable-samples --disable-device --disable-webserver --without-documentation
#ifdef HAVE_IOS
#PUPNP_OPTIONS+= --disable-reuseaddr
#else
#ifdef HAVE_MACOSX
#PUPNP_OPTIONS+= --disable-reuseaddr
#endif
#endif

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
