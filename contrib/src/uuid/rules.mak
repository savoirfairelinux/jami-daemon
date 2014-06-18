# libuuid part of util-linux

UUID_VERSION := 1.0.2
UUID_URL := $(SF)/libuuid/libuuid-$(UUID_VERSION).tar.gz

ifeq ($(call need_pkg," >= 2.0.0"),)
PKGS_FOUND += uuid
endif

$(TARBALLS)/libuuid-$(UUID_VERSION).tar.gz:
	$(call download,$(UUID_URL))

.sum-uuid: libuuid-$(UUID_VERSION).tar.gz

uuid: libuuid-$(UUID_VERSION).tar.gz .sum-uuid
	$(UNPACK)
	$(APPLY) $(SRC)/uuid/android.patch
	$(MOVE)

.uuid: uuid
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
