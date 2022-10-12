# x264
X264_HASH := ed0f7a634050a62c1da27c99eea710824d4c3705
X264_GITURL := https://code.videolan.org/videolan/x264.git

ifeq ($(call need_pkg,"x264 >= 0.86"),)
PKGS_FOUND += x264
endif

X264CONF = --prefix="$(PREFIX)" \
           --host="$(HOST)"     \
           --enable-static      \
           --disable-avs        \
           --disable-lavf       \
           --disable-cli        \
           --disable-ffms       \
           --disable-opencl

ifndef HAVE_WIN32
X264CONF += --enable-pic
else
X264CONF += --enable-win32thread
endif
ifndef HAVE_IOS
ifndef HAVE_ANDROID
ifndef HAVE_MACOSX
ifdef HAVE_CROSS_COMPILE
X264CONF += --cross-prefix="$(CROSS_COMPILE)"
endif
endif
endif
endif

# android x86_64 has reloc errors related to assembly optimizations
ifdef HAVE_ANDROID
ifeq ($(ARCH),x86_64)
X264CONF += --disable-asm
endif
endif

$(TARBALLS)/x264-$(X264_HASH).tar.xz:
	$(call download_git,$(X264_GITURL),master,$(X264_HASH))

.sum-x264: x264-$(X264_HASH).tar.xz
	$(warning $@ not implemented)
	touch $@

x264: x264-$(X264_HASH).tar.xz .sum-x264
	rm -Rf $@-$(X264_HASH)
	mkdir -p $@-$(X264_HASH)
	(cd $@-$(X264_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f $<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.x264: x264
ifdef HAVE_ANDROID
	cd $< && $(HOSTVARS) AS="$(CC)" ./configure $(X264CONF)
else
ifeq ($(IOS_TARGET_PLATFORM),iPhoneOS)
	cd $< && $(HOSTVARS) ASFLAGS="$(CFLAGS)" ./configure $(X264CONF)
else
	cd $< && $(HOSTVARS) ./configure $(X264CONF)
endif
endif
	cd $< && $(MAKE) install
	touch $@
