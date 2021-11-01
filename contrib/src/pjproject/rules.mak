# PJPROJECT
PJPROJECT_VERSION := e1f389d0b905011e0cb62cbdf7a8b37fc1bcde1a
PJPROJECT_URL := https://github.com/savoirfairelinux/pjproject/archive/${PJPROJECT_VERSION}.tar.gz

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
	$(APPLY) $(SRC)/pjproject/0009-add-config-site.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/pjproject/0001-android.patch
endif
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
