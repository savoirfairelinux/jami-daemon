# PJPROJECT
PJPROJECT_VERSION := 6b9212dcb4b3f781c1e922ae544b063880bc46ac
PJPROJECT_URL := https://github.com/pjsip/pjproject/archive/$(PJPROJECT_VERSION).tar.gz

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
                     --disable-silk         \
                     --disable-sdl          \
                     --disable-ffmpeg       \
                     --disable-v4l2         \
                     --disable-openh264     \
                     --disable-resample     \
                     --disable-libwebrtc    \
                     --with-gnutls=$(PREFIX)

PJPROJECT_EXTRA_CFLAGS = -g -DPJ_ENABLE_EXTRA_CHECK=1 -DPJ_ICE_MAX_CAND=256 -DPJ_ICE_MAX_CHECKS=1024 -DPJ_ICE_COMP_BITS=2 -DPJ_ICE_MAX_STUN=6 -DPJSIP_MAX_PKT_LEN=8000 -DPJ_ICE_ST_MAX_CAND=32 -DPJ_HAS_TCP=1
PJPROJECT_EXTRA_CXXFLAGS = -g -DPJ_ENABLE_EXTRA_CHECK=1 -DPJ_ICE_MAX_CAND=256 -DPJ_ICE_MAX_CHECKS=1024 -DPJ_ICE_COMP_BITS=2 -DPJ_ICE_MAX_STUN=6 -DPJSIP_MAX_PKT_LEN=8000 -DPJ_ICE_ST_MAX_CAND=32 -DPJ_HAS_TCP=1

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

$(TARBALLS)/pjproject-$(PJPROJECT_VERSION).tar.gz:
	$(call download,$(PJPROJECT_URL))

.sum-pjproject: pjproject-$(PJPROJECT_VERSION).tar.gz

pjproject: pjproject-$(PJPROJECT_VERSION).tar.gz .sum-pjproject
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/pjproject/pj_win.patch
endif
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/pjproject/android.patch
endif
	$(APPLY) $(SRC)/pjproject/rfc2466.patch
	$(APPLY) $(SRC)/pjproject/ipv6.patch
	$(APPLY) $(SRC)/pjproject/multiple_listeners.patch
	$(APPLY) $(SRC)/pjproject/pj_ice_sess.patch
	$(APPLY) $(SRC)/pjproject/fix_turn_fallback.patch
	$(APPLY) $(SRC)/pjproject/fix_ioqueue_ipv6_sendto.patch
	$(APPLY) $(SRC)/pjproject/add_dtls_transport.patch
	$(APPLY) $(SRC)/pjproject/rfc6062.patch
	$(APPLY) $(SRC)/pjproject/rfc6544.patch
	$(APPLY) $(SRC)/pjproject/ice_config.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.pjproject: pjproject
ifdef HAVE_IOS
	cd $< && ARCH="-arch $(ARCH)" IPHONESDK=$(IOS_SDK) $(HOSTVARS) EXCLUDE_APP=1 ./configure-iphone $(HOSTCONF) $(PJPROJECT_OPTIONS)
else
	cd $< && $(HOSTVARS) EXCLUDE_APP=1 ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
endif
	cd $< && CFLAGS="$(PJPROJECT_EXTRA_CFLAGS)" CXXFLAGS="$(PJPROJECT_EXTRA_CXXFLAGS)" EXCLUDE_APP=1 $(MAKE) && $(MAKE) install
	touch $@
