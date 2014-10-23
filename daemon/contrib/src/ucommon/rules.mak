# UCOMMON
UCOMMON_VERSION := 6.2.0
UCOMMON_URL := $(GNUTELEPHONY)/ucommon/archive/v$(UCOMMON_VERSION).tar.gz

UCOMMON_OPTIONS := --enable-stdcpp --with-pkg-config --disable-utils --disable-tests

ifeq ($(call need_pkg,'ucommon >= 6.0.0 commoncpp >= 6.0.0'),)
PKGS_FOUND += ucommon
endif

DEPS_ucommon = gnutls

$(TARBALLS)/ucommon-$(UCOMMON_VERSION).tar.gz:
	$(call download,$(UCOMMON_URL))

.sum-ucommon: ucommon-$(UCOMMON_VERSION).tar.gz

ucommon: ucommon-$(UCOMMON_VERSION).tar.gz .sum-ucommon
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && autoreconf -fi
	$(MOVE)

.ucommon: ucommon
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(UCOMMON_OPTIONS)
	cd $< && $(MAKE) install
	touch $@
