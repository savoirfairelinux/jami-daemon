# PJPROJECT
PJPROJECT_VERSION := 2.9
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
	$(APPLY) $(SRC)/pjproject/0017-android.patch.patch
endif
	$(APPLY) $(SRC)/pjproject/0001-rfc6544.patch.patch
	$(APPLY) $(SRC)/pjproject/0002-fix_turn_alloc_failure.patch.patch
	$(APPLY) $(SRC)/pjproject/0003-rfc2466.patch.patch
	$(APPLY) $(SRC)/pjproject/0004-ipv6.patch.patch
	$(APPLY) $(SRC)/pjproject/0005-multiple_listeners.patch.patch
	$(APPLY) $(SRC)/pjproject/0006-pj_ice_sess.patch.patch
	$(APPLY) $(SRC)/pjproject/0007-fix_turn_fallback.patch.patch
	$(APPLY) $(SRC)/pjproject/0008-fix_ioqueue_ipv6_sendto.patch.patch
	$(APPLY) $(SRC)/pjproject/0009-add_dtls_transport.patch.patch
	$(APPLY) $(SRC)/pjproject/0010-ice_config.patch.patch
	$(APPLY) $(SRC)/pjproject/0011-sip_config.patch.patch
	$(APPLY) $(SRC)/pjproject/0012-fix_first_packet_turn_tcp.patch.patch
	$(APPLY) $(SRC)/pjproject/0013-fix_ebusy_turn.patch.patch
	$(APPLY) $(SRC)/pjproject/0014-ignore_ipv6_on_transport_check.patch.patch
	$(APPLY) $(SRC)/pjproject/0015-disable_local_resolution.patch.patch
	$(APPLY) $(SRC)/pjproject/0016-fix_assert_on_connection_attempt.patch.patch
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
