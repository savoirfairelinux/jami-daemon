# CCRTP
CCRTP_VERSION := 2.0.9
CCRTP_URL := $(GNUTELEPHONY)/ccrtp/archive/v$(CCRTP_VERSION).tar.gz

PKGS += ccrtp
ifeq ($(call need_pkg,'libccrtp >= 2.0.3'),)
PKGS_FOUND += ccrtp
endif

DEPS_ccrtp = ucommon gcrypt gnutls

$(TARBALLS)/ccrtp-$(CCRTP_VERSION).tar.gz:
	$(call download,$(CCRTP_URL))

.sum-ccrtp: ccrtp-$(CCRTP_VERSION).tar.gz

ccrtp: ccrtp-$(CCRTP_VERSION).tar.gz .sum-ccrtp
	$(UNPACK)
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/ucommon/osx-autogen.patch
endif
	$(APPLY) $(SRC)/ccrtp/standardheader.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && env NOCONFIGURE=1 sh autogen.sh
	$(MOVE)

.ccrtp: ccrtp
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
