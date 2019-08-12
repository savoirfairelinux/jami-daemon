# PJPROJECT
PJPROJECT_VERSION := 5dfa75be7d69047387f9b0436dd9492bbbf03fe4
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
	$(APPLY) $(SRC)/pjproject/fix_turn_alloc_failure.patch
	$(APPLY) $(SRC)/pjproject/rfc2466.patch
	$(APPLY) $(SRC)/pjproject/ipv6.patch
	$(APPLY) $(SRC)/pjproject/multiple_listeners.patch
	$(APPLY) $(SRC)/pjproject/pj_ice_sess.patch
	$(APPLY) $(SRC)/pjproject/fix_turn_fallback.patch
	$(APPLY) $(SRC)/pjproject/fix_ioqueue_ipv6_sendto.patch
	$(APPLY) $(SRC)/pjproject/add_dtls_transport.patch
	$(APPLY) $(SRC)/pjproject/rfc6544.patch
	$(APPLY) $(SRC)/pjproject/ice_config.patch
	$(APPLY) $(SRC)/pjproject/sip_config.patch
	$(APPLY) $(SRC)/pjproject/fix_first_packet_turn_tcp.patch
	$(APPLY) $(SRC)/pjproject/fix_ebusy_turn.patch
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
