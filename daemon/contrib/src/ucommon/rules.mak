# UCOMMON
UCOMMON_VERSION := 6.1.8
UCOMMON_URL := https://github.com/dyfet/ucommon/archive/v$(UCOMMON_VERSION).tar.gz

ifeq ($(call need_pkg,'ucommon >= 6.0.0 commoncpp >= 6.0.0'),)
PKGS_FOUND += ucommon
endif

DEPS_ucommon = gnutls

$(TARBALLS)/ucommon-$(UCOMMON_VERSION).tar.gz:
	$(call download,$(UCOMMON_URL))

.sum-ucommon: ucommon-$(UCOMMON_VERSION).tar.gz

ucommon: ucommon-$(UCOMMON_VERSION).tar.gz .sum-ucommon
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/ucommon/windows_platform.patch
endif
	$(APPLY) $(SRC)/ucommon/extended.patch
	$(APPLY) $(SRC)/ucommon/usedefines.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && env NOCONFIGURE=1 sh autogen.sh
	$(MOVE)

UCOMMON_CONF := --enable-stdcpp --with-pkg-config

.ucommon: ucommon
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(UCOMMON_CONF)
	cd $< && $(MAKE) install
	touch $@
