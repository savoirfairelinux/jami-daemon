# PJPROJECT
PJPROJECT_VERSION := 2.10
PJPROJECT_URL := https://github.com/pjsip/pjproject/archive/$(PJPROJECT_VERSION).tar.gz

PJPROJECT_OPTIONS := --disable-sound        \
                     --enable-video         \
                     --enable-ext-sound     \
                     --disable-speex-aec    \
                     --disable-g711-codec   \
                     --disable-l16-codec    \
                     --disable-gsm-codec    \
                     --disable-g722-codec   \
                     --disable-g7221-codec  \
                     --disable-speex-codec  \
                     --disable-ilbc-codec   \
                     --disable-opencore-amr \
                     --disable-silk         \
                     --disable-sdl          \
                     --disable-ffmpeg       \
                     --disable-v4l2         \
                     --disable-openh264     \
                     --disable-resample     \
                     --disable-libwebrtc    \
                     --with-gnutls=$(PREFIX)

PKGS += pjproject
# FIXME: nominally 2.2.0 is enough, but it has to be patched for gnutls
ifeq ($(call need_pkg,'libpjproject'),)
PKGS_FOUND += pjproject
endif

DEPS_pjproject += gnutls
ifndef HAVE_MACOSX
DEPS_pjproject += uuid
endif

ifdef HAVE_LINUX
PJPROJECT_OPTIONS += --enable-epoll
endif

$(TARBALLS)/pjproject-$(PJPROJECT_VERSION).tar.gz:
	$(call download,$(PJPROJECT_URL))

.sum-pjproject: pjproject-$(PJPROJECT_VERSION).tar.gz

pjproject: pjproject-$(PJPROJECT_VERSION).tar.gz .sum-pjproject
	$(UNPACK)
	$(APPLY) $(SRC)/pjproject/0001-rfc6544.patch
	$(APPLY) $(SRC)/pjproject/0002-rfc2466.patch
	$(APPLY) $(SRC)/pjproject/0003-add-tcp-keep-alive.patch
	$(APPLY) $(SRC)/pjproject/0004-allow-multiple-listeners.patch
	$(APPLY) $(SRC)/pjproject/0005-fix-ebusy-turn.patch
	$(APPLY) $(SRC)/pjproject/0006-ignore-ipv6-on-transport-check.patch
	$(APPLY) $(SRC)/pjproject/0007-pj-ice-sess.patch
	$(APPLY) $(SRC)/pjproject/0008-fix-ioqueue-ipv6-sendto.patch
	$(APPLY) $(SRC)/pjproject/0009-icest-add-on_valid_pair-to-ice-strans-callbacks.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/pjproject/0001-android.patch
endif
	cp $(SRC)/pjproject/config_site_main.h pjproject-$(PJPROJECT_VERSION)/pjlib/include/pj/config_site.h
	cp $(SRC)/pjproject/user.mak pjproject-$(PJPROJECT_VERSION)/
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.pjproject: pjproject
ifdef HAVE_IOS
	cd $< && ARCH="-arch $(ARCH)" IPHONESDK=$(IOS_SDK) $(HOSTVARS) EXCLUDE_APP=1 ./configure-iphone $(HOSTCONF) $(PJPROJECT_OPTIONS)
else
	cd $< && $(HOSTVARS) EXCLUDE_APP=1 ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
endif
	cd $< && EXCLUDE_APP=1 $(MAKE) && $(MAKE) install
	touch $@
