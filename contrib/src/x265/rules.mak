# x265
X265_VERSION := 3.1.1
X265_GITURL := ftp.videolan.org/pub/videolan/x265/x265_$(X265_VERSION).tar.gz

ifeq ($(call need_pkg,"x265 >= 3.1.1" || call need_pkg,"libnuma-dev"),)
PKGS_FOUND += x265
endif

X265CONF = -DCMAKE_INSTALL_PREFIX="$(PREFIX)" \
		   -DENABLE_SHARED:bool=off \
		   -DENABLE_CLI=Off

ifndef HAVE_IOS
ifdef HAVE_CROSS_COMPILE
X265CONF += --cross-prefix="$(CROSS_COMPILE)"
endif
endif

# android x86_64 has reloc errors related to assembly optimizations
ifdef HAVE_ANDROID
ifeq ($(ARCH),x86_64)
X265CONF += --disable-asm
endif
endif

$(TARBALLS)/x265-$(X265_VERSION).tar.xz:
	$(call download,$(X265_GITURL))

.sum-x265: x265-$(X265_VERSION).tar.xz

x265: x265-$(X265_VERSION).tar.xz .sum-x265
	rm -Rf $@-$(X265_VERSION)
	mkdir -p $@-$(X265_VERSION)
	(cd $@-$(X265_VERSION) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f ../$<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.x265: x265
	cd $< && $(HOSTVARS) cmake "Unix Makefiles" source $(X265CONF)
	cd $< && $(MAKE) install
	touch $@
