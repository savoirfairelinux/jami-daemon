# x264
ifndef HAVE_IOS
X264_HASH := 8c2974255b01728d4eda2434cc1997c4a3ca5eff
else
X264_HASH := fa3cac516cb71b8ece09cedbfd0ce631ca8a2a4c
endif
X264_GITURL := git://git.videolan.org/x264.git

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
ifdef HAVE_CROSS_COMPILE
X264CONF += --cross-prefix="$(CROSS_COMPILE)"
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
	(cd $@-$(X264_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f ../$<)
ifdef HAVE_IOS
	$(APPLY) $(SRC)/x264/remove-align.patch
endif
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.x264: x264
	cd $< && $(HOSTVARS) ./configure $(X264CONF)
	cd $< && $(MAKE) install
	touch $@
