# GNU Guile

GUILE_VERSION := 3.0.7
GUILE_URL     := https://ftp.gnu.org/gnu/guile/guile-${GUILE_VERSION}.tar.gz

ifeq ($(call need_pkg "libguile-3.0 >= 3.0.7"),)
PKGS_FOUND += guile
endif

DEPS_guile = gmp iconv libunistring libffi libgc

$(TARBALLS)/guile-$(GUILE_VERSION).tar.gz:
	$(call download,$(GUILE_URL))

.sum-guile: guile-$(GUILE_VERSION).tar.gz

guile: guile-$(GUILE_VERSION).tar.gz .sum-guile
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

GUILE_CONF :=            \
	$(HOSTCONF)      \
	--disable-static \
	--enable-shared

.guile: guile
	cd $< && $(HOSTVARS) ./configure $(GUILE_CONF)
	cd $< && $(MAKE) install
	touch $@
