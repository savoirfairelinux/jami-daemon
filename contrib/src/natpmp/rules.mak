# libnatpmp
NATPMP_VERSION := 20150609
NATPMP_URL := http://miniupnp.free.fr/files/download.php?file=libnatpmp-$(NATPMP_VERSION).tar.gz

ifndef HAVE_WIN32
ifndef HAVE_ANDROID
ifndef HAVE_IOS
ifdef HAVE_MACOSX
ifeq ($(ARCH),x86_64)
PKGS += natpmp
endif
else
PKGS += natpmp
endif
endif
endif
endif

ifeq ($(call need_pkg,'libnatpmp'),)
PKGS_FOUND += natpmp
endif

$(TARBALLS)/libnatpmp-$(NATPMP_VERSION).tar.gz:
	$(call download,$(NATPMP_URL))

.sum-natpmp: libnatpmp-$(NATPMP_VERSION).tar.gz

natpmp: libnatpmp-$(NATPMP_VERSION).tar.gz .sum-natpmp
	$(UNPACK)
	$(MOVE)

.natpmp: natpmp
	cd $< && $(MAKE) INSTALLPREFIX="$(PREFIX)" $(HOSTVARS) install
	-rm -f $(PREFIX)/lib/libnatpmp.so* $(PREFIX)/lib/libnatpmp.dylib*
	touch $@
