# PJPROJECT
PJPROJECT_VERSION := 2.4.5
PJPROJECT_URL := http://www.pjsip.org/release/$(PJPROJECT_VERSION)/pjproject-$(PJPROJECT_VERSION).tar.bz2

PJPROJECT_OPTIONS := --disable-oss          \
                     --disable-sound        \
                     --disable-video        \
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
                     --disable-sdl          \
                     --disable-ffmpeg       \
                     --disable-v4l2         \
                     --enable-ssl=gnutls

ifdef HAVE_ANDROID
PJPROJECT_OPTIONS += --with-ssl=$(PREFIX)
endif
ifdef HAVE_WIN32
PJPROJECT_OPTIONS += --with-ssl=$(PREFIX)
endif
ifdef HAVE_IOS
PJPROJECT_OPTIONS += --with-ssl=$(PREFIX)
endif

PJPROJECT_EXTRA_CFLAGS = -DPJ_ICE_MAX_CAND=32 -DPJ_ICE_MAX_CHECKS=150 -DPJ_ICE_COMP_BITS=2

ifdef HAVE_WIN64
PJPROJECT_EXTRA_CFLAGS += -DPJ_WIN64=1
endif

PKGS += pjproject
# FIXME: nominally 2.2.0 is enough, but it has to be patched for gnutls
ifeq ($(call need_pkg,'libpjproject'),)
PKGS_FOUND += pjproject
endif

DEPS_pjproject += gnutls
ifndef HAVE_WIN32
ifndef HAVE_MACOSX
DEPS_pjproject += uuid
endif
endif

$(TARBALLS)/pjproject-$(PJPROJECT_VERSION).tar.bz2:
	$(call download,$(PJPROJECT_URL))

.sum-pjproject: pjproject-$(PJPROJECT_VERSION).tar.bz2

pjproject: pjproject-$(PJPROJECT_VERSION).tar.bz2 .sum-pjproject
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/pjproject/intptr_t.patch
	$(APPLY) $(SRC)/pjproject/pj_win.patch
endif
	$(APPLY) $(SRC)/pjproject/endianness.patch
	$(APPLY) $(SRC)/pjproject/gnutls.patch
	$(APPLY) $(SRC)/pjproject/notestsapps.patch
	$(APPLY) $(SRC)/pjproject/ipv6.patch
	$(APPLY) $(SRC)/pjproject/ice_config.patch
	$(APPLY) $(SRC)/pjproject/multiple_listeners.patch
	$(APPLY) $(SRC)/pjproject/pj_ice_sess.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.pjproject: pjproject
ifdef HAVE_IOS
	cd $< && ARCH="-arch $(ARCH)" IPHONESDK=$(IOS_SDK) $(HOSTVARS) ./configure-iphone $(HOSTCONF) $(PJPROJECT_OPTIONS)
else
	cd $< && $(HOSTVARS) ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
endif
	cd $< && CFLAGS="$(PJPROJECT_EXTRA_CFLAGS)" $(MAKE) && $(MAKE) install
	touch $@
