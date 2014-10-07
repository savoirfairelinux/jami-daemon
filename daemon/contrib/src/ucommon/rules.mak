# UCOMMON
UCOMMON_VERSION := 6.1.8
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
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/ucommon/osx-linked.patch
	$(APPLY) $(SRC)/ucommon/osx-unicode.patch
endif
	$(APPLY) $(SRC)/ucommon/extended.patch
	$(APPLY) $(SRC)/ucommon/usedefines.patch
	$(APPLY) $(SRC)/ucommon/any-addr-and-overloads.patch
	$(APPLY) $(SRC)/ucommon/skip_programs.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && autoreconf -fi
	$(MOVE)

.ucommon: ucommon
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(UCOMMON_OPTIONS)
	cd $< && $(MAKE) install
	touch $@
