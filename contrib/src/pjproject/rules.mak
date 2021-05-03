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
	$(APPLY) $(SRC)/pjproject/0004-multiple_listeners.patch
	$(APPLY) $(SRC)/pjproject/0005-fix_ebusy_turn.patch
	$(APPLY) $(SRC)/pjproject/0006-ignore_ipv6_on_transport_check.patch
	$(APPLY) $(SRC)/pjproject/0007-upnp-srflx-nat-assisted-cand.patch
	$(APPLY) $(SRC)/pjproject/0008-fix_ioqueue_ipv6_sendto.patch
	$(APPLY) $(SRC)/pjproject/0009-add-config-site.patch
	$(APPLY) $(SRC)/pjproject/0010-fix-pkgconfig.patch
	$(APPLY) $(SRC)/pjproject/0011-fix-tcp-death-detection.patch
	$(APPLY) $(SRC)/pjproject/0012-fix-turn-shutdown-crash.patch
	$(APPLY) $(SRC)/pjproject/0013-Assign-unique-local-preferences-for-candidates-with-.patch
	$(APPLY) $(SRC)/pjproject/0014-Add-new-compile-time-setting-PJ_ICE_ST_USE_TURN_PERM.patch
	$(APPLY) $(SRC)/pjproject/0015-update-local-preference-for-peer-reflexive-candidate.patch
	$(APPLY) $(SRC)/pjproject/0016-use-addrinfo-instead-CFHOST.patch
	$(APPLY) $(SRC)/pjproject/0017-CVE-2020-15260.patch
	$(APPLY) $(SRC)/pjproject/0018-CVE-2021-21375.patch
	$(APPLY) $(SRC)/pjproject/0019-ignore-down-interfaces.patch
	$(APPLY) $(SRC)/pjproject/RFC7335.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/pjproject/0001-android.patch
endif
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.pjproject: pjproject
	cd $< && autoconf -o aconfigure aconfigure.ac # for 0010-fix-pkgconfig.patch
ifdef HAVE_IOS
	cd $< && ARCH="-arch $(ARCH)" IPHONESDK=$(IOS_SDK) $(HOSTVARS) EXCLUDE_APP=1 ./configure-iphone $(HOSTCONF) $(PJPROJECT_OPTIONS)
else
	cd $< && $(HOSTVARS) EXCLUDE_APP=1 ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
endif
	cd $< && EXCLUDE_APP=1 $(MAKE) && $(MAKE) install
	touch $@
