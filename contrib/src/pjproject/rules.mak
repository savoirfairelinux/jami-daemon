# PJPROJECT
PKG_NAME = pjproject

PJPROJECT_VERSION := 5dfa75be7d69047387f9b0436dd9492bbbf03fe4
PJPROJECT_URL     := https://github.com/pjsip/pjproject/archive/$(PJPROJECT_VERSION).tar.gz
PJ_GIT_URL        := https://github.com/pjsip/pjproject.git
PJ_GIT_BRANCH     := master
PJ_GIT_COMMIT     := 3e7b75cb2e482baee58c1991bd2fa4fb06774e0d

PJPROJECT_OPTIONS := --disable-sound        \
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
PJPROJECT_OPTIONS += --enable-epoll
endif
endif

$(TARBALLS)/$(PKG_NAME)-$(PJPROJECT_VERSION).tar.gz:
	$(call download,$(PJPROJECT_URL))

$(GITREPOS)/pjproject.git:
	$(call download_git2,$(PJ_GIT_BRANCH),$(PJ_GIT_URL))

.sum-pjproject: $(GITREPOS)/pjproject.git
	$(call gitverify,$<,$(PJ_GIT_COMMIT))
	touch $@

pjproject: $(GITREPOS)/$(PKG_NAME).git
	$(MOVE_GIT)
	$(UPDATE_AUTOCONFIG)
ifdef HAVE_WIN32
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/pj_win.patch
endif
ifdef HAVE_ANDROID
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/android.patch
endif
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0001-rfc6544.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0002-fix_ebusy_turn.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0003-add_tcp_keep_alive.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0004-enable-ipv6.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0005-multiple_listeners.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0006-pj_ice_sess.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0007-fix_ioqueue_ipv6_sendto.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0008-ice_config.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0009-sip_config.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0010-disable_local_resolution.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0011-fix_turn_fallback.patch
	$(GIT_APPLY) $(SRC)/$(PKG_NAME)/0012-add-compid-argument-to-pj_ice_strans_cb.on_data_sent.patch

.pjproject: pjproject
ifdef HAVE_IOS
	cd $< && ARCH="-arch $(ARCH)" IPHONESDK=$(IOS_SDK) $(HOSTVARS) EXCLUDE_APP=1 ./configure-iphone $(HOSTCONF) $(PJPROJECT_OPTIONS)
else
	cd $< && $(HOSTVARS) EXCLUDE_APP=1 ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
endif
	cd $< && $(HOSTVARS) EXCLUDE_APP=1 $(MAKE) -j && $(MAKE) install
	touch $@
