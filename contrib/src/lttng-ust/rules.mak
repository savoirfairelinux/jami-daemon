# lttng-ust

LTTNG_UST_VERSION := 2.13.1
LTTNG_UST_URL     := https://lttng.org/files/lttng-ust/lttng-ust-${LTTNG_UST_VERSION}.tar.bz2

ifeq ($(call need_pkg "liblttng-ust >= 2.13.0"),)
PKGS_FOUND += lttng-ust
endif

DEPS_lttng_ust=

$(TARBALLS)/lttng-ust-$(LTTNG_UST_VERSION).tar.bz2:
	$(call download,$(LTTNG_UST_URL))

.sum-lttng-ust: lttng-ust-$(LTTNG_UST_VERSION).tar.bz2

lttng-ust: lttng-ust-$(LTTNG_UST_VERSION).tar.bz2 .sum-lttng-ust
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

LTTNG_UST_CONF :=           \
	--disable-man-pages \
	--disable-numa      \
	$(HOSTCONF)         \
	--enable-shared

.lttng-ust: lttng-ust
	cd $< && $(HOSTVARS) ./configure $(LTTNG_UST_CONF)
	cd $< && $(MAKE) install
	touch $@
