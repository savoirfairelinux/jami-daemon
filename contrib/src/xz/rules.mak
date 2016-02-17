# xz

XZ_VERSION := 5.2.2
XZ_URL := http://tukaani.org/xz/xz-${XZ_VERSION}.tar.gz

ifeq ($(call need_pkg,"xz >= 5.2.2"),)
PKGS_FOUND += xz
endif

XZCONF = --prefix="$(PREFIX)"   \
           --host="$(HOST)"     \
           --enable-static=yes  \
           --enable-shared=no

ifndef HAVE_WIN32
XZCONF += --with-pic
endif

$(TARBALLS)/xz-$(XZ_VERSION).tar.gz:
	$(call download,$(XZ_URL))

.sum-xz: xz-$(XZ_VERSION).tar.gz

xz: xz-$(XZ_VERSION).tar.gz .sum-xz
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)


.xz: xz
	cd $< && $(HOSTVARS) ./configure $(XZCONF)
	cd $< && $(MAKE) install
	touch $@
