# libunistring

LIBUNISTRING_VERSION := 0.9.10
LIBUNISTRING_URL     := https://ftp.gnu.org/gnu/libunistring/libunistring-${LIBUNISTRING_VERSION}.tar.gz

DEPS_libunistring=

$(TARBALLS)/libunistring-$(LIBUNISTRING_VERSION).tar.gz:
	$(call download,$(LIBUNISTRING_URL))

.sum-libunistring: libunistring-$(LIBUNISTRING_VERSION).tar.gz

libunistring: libunistring-$(LIBUNISTRING_VERSION).tar.gz .sum-libunistring
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

LIBUNISTRING_CONF :=     \
	$(HOSTCONF)      \
	--disable-static \
	--enable-shared

.libunistring: libunistring
	cd $< && $(HOSTVARS) ./configure $(LIBUNISTRING_CONF)
	cd $< && $(MAKE) install
	touch $@
