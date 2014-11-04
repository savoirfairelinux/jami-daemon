# CCRTP
CCRTP_VERSION := 2.1.0
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
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.ccrtp: ccrtp
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
