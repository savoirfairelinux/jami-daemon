# CCRTP
CCRTP_VERSION := 2.0.9
CCRTP_URL := $(GNUTELEPHONY)/ccrtp-$(CCRTP_VERSION).tar.gz

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
	$(APPLY) $(SRC)/ccrtp/standardheader.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

CCRTP_CONF :=

.ccrtp: ccrtp
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(CCRTP_CONF)
	cd $< && $(MAKE) install
	touch $@
