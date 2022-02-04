# liburcu

LIBURCU_VERSION := 0.13.1
LIBURCU_URL     := https://lttng.org/files/urcu/userspace-rcu-${LIBURCU_VERSION}.tar.bz2

ifeq ($(call need_pkg "liburcu >= 0.13.1"),)
PKGS_FOUND += liburcu
endif

$(TARBALLS)/liburcu-$(LIBURCU_VERSION).tar.bz2:
	$(call download,$(LIBURCU_URL))

.sum-liburcu: liburcu-$(LIBURCU_VERSION).tar.bz2

liburcu: liburcu-$(LIBURCU_VERSION).tar.bz2 .sum-liburcu
	$(UNPACK)
	mv userspace-rcu-$(LIBURCU_VERSION) liburcu-$(LIBURCU_VERSION)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.liburcu: liburcu
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
