# UCOMMON
UCOMMON_VERSION := 6.2.0
UCOMMON_URL := $(GNUTELEPHONY)/ucommon/archive/v$(UCOMMON_VERSION).tar.gz

UCOMMON_OPTIONS := --enable-stdcpp --with-pkg-config --disable-utils --disable-tests

ifeq ($(call need_pkg,'ucommon >= ${UCOMMON_VERSION} commoncpp >= ${UCOMMON_VERSION}'),)
PKGS_FOUND += ucommon
endif

DEPS_ucommon = gnutls

$(TARBALLS)/ucommon-$(UCOMMON_VERSION).tar.gz:
	$(call download,$(UCOMMON_URL))

.sum-ucommon: ucommon-$(UCOMMON_VERSION).tar.gz

ucommon: ucommon-$(UCOMMON_VERSION).tar.gz .sum-ucommon
	$(UNPACK)
	$(APPLY) $(SRC)/ucommon/noexcept.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/ucommon/windows_platform.patch
endif
	$(APPLY) $(SRC)/ucommon/fsys.patch
	$(APPLY) $(SRC)/ucommon/datetime.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && autoreconf -fi
	$(MOVE)

.ucommon: ucommon
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(UCOMMON_OPTIONS)
	cd $< && $(MAKE) install
	touch $@
