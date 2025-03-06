# PJPROJECT
PJPROJECT_VERSION := 37130c943d59f25a71935803ea2d84515074a237
PJPROJECT_URL := https://github.com/savoirfairelinux/pjproject/archive/${PJPROJECT_VERSION}.tar.gz

PJPROJECT_OPTIONS := --disable-sound        \
                     --enable-video         \
                     --enable-ext-sound     \
                     --disable-android-mediacodec \
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
                     --with-gnutls=yes

PKGS += pjproject

ifeq ($(call need_pkg,'libpjproject'),)
PKGS_FOUND += pjproject
endif

DEPS_pjproject += gnutls

ifdef HAVE_LINUX
PJPROJECT_OPTIONS += --enable-epoll
endif

$(TARBALLS)/pjproject-$(PJPROJECT_VERSION).tar.gz:
	$(call download,$(PJPROJECT_URL))

.sum-pjproject: pjproject-$(PJPROJECT_VERSION).tar.gz

pjproject: pjproject-$(PJPROJECT_VERSION).tar.gz .sum-pjproject
	$(UNPACK)
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/pjproject/0001-android.patch
endif
ifdef HAVE_IOS
	$(APPLY) $(SRC)/pjproject/0003-disable-ios-pointtopoint.patch
	$(APPLY) $(SRC)/pjproject/0004-ios-16.patch
endif
	$(UPDATE_AUTOCONFIG)
	$(MOVE)
.pjproject: pjproject
ifdef HAVE_IOS
    # provide DEVPATH and MIN_IOS build arm64 simulator
	cd $< && ARCH="-arch $(ARCH)" IPHONESDK=$(IOS_SDK) $(HOSTVARS) EXCLUDE_APP=1 DEVPATH="$(DEVPATH)" MIN_IOS="$(MIN_IOS)" ./configure-iphone $(HOSTCONF) $(PJPROJECT_OPTIONS)
else ifdef HAVE_MACOSX
	cd $< && ARCH="-arch $(ARCH)" $(HOSTVARS) EXCLUDE_APP=1 ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
else
	cd $< && $(HOSTVARS) EXCLUDE_APP=1 ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
endif
	cd $< && EXCLUDE_APP=1 $(MAKE) && $(MAKE) install
	touch $@
