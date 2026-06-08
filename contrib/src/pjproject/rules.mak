# PJPROJECT
PJPROJECT_VERSION := 00ce02ff8c0c16d3570f7c33659c56b8b4dfebb9
PJPROJECT_URL := https://github.com/savoirfairelinux/pjproject/archive/${PJPROJECT_VERSION}.tar.gz

# pjproject's aconfigure locates the GnuTLS *library* through pkg-config, but it
# probes the GnuTLS *header* under "<prefix>/include", where <prefix> is the value
# passed to --with-gnutls (it adds -I<prefix>/include unconditionally).
# Resolve the real GnuTLS prefix via pkg-config so both the header and library checks succeed;
# fall back to the contrib prefix for cross builds where GnuTLS is built into contrib.
ifndef IGNORE_SYSTEM_LIBS
PJPROJECT_GNUTLS_PREFIX := $(shell $(PKG_CONFIG) --variable=prefix gnutls 2>/dev/null)
endif
PJPROJECT_GNUTLS_PREFIX ?= $(PREFIX)

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
                     --with-gnutls=$(PJPROJECT_GNUTLS_PREFIX)

PKGS += pjproject

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
    # provide DEVPATH and MIN_IOS to build arm64 simulator
	cd $< && ARCH="-arch $(ARCH)" IPHONESDK=$(IOS_SDK) $(HOSTVARS) EXCLUDE_APP=1 DEVPATH="$(DEVPATH)" MIN_IOS="$(MIN_IOS)" ./configure-iphone $(HOSTCONF) $(PJPROJECT_OPTIONS)
else ifdef HAVE_MACOSX
	cd $< && ARCH="-arch $(ARCH)" $(HOSTVARS) EXCLUDE_APP=1 ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
else
	cd $< && $(HOSTVARS) EXCLUDE_APP=1 ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
endif
	cd $< && EXCLUDE_APP=1 $(MAKE) && $(MAKE) install
	touch $@
