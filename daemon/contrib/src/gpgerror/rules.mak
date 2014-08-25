# GPG-error library

GPGERROR_VERSION := 1.13
GPGERROR_URL := http://ftp.heanet.ie/mirrors/ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-$(GPGERROR_VERSION).tar.bz2

ifeq ($(call need_pkg," >= 1.11"),)
PKGS_FOUND += gpgerror
endif

$(TARBALLS)/libgpg-error-$(GPGERROR_VERSION).tar.bz2:
	$(call download,$(GPGERROR_URL))

.sum-gpgerror: libgpg-error-$(GPGERROR_VERSION).tar.bz2

gpgerror: libgpg-error-$(GPGERROR_VERSION).tar.bz2 .sum-gpgerror
	$(UNPACK)
	$(MOVE)

.gpgerror: gpgerror
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
