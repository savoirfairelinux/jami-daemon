# UCOMMON
UCOMMON_VERSION := 6.2.1
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
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/ucommon/windows_platform.patch
endif
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.ucommon: ucommon
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(UCOMMON_OPTIONS)
	cd $< && $(MAKE) install
	touch $@
