# PJPROJECT
PJPROJECT_VERSION := 2.2.1
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

PKGS += pjproject
# FIXME: nominally 2.2.0 is enough, but it has to be patched for gnutls
ifeq ($(call need_pkg,'libpjproject'),)
PKGS_FOUND += pjproject
endif

DEPS_pjproject += gnutls
ifndef HAVE_MACOSX
DEPS_pjproject += uuid
endif

$(TARBALLS)/pjproject-$(PJPROJECT_VERSION).tar.bz2:
	$(call download,$(PJPROJECT_URL))

.sum-pjproject: pjproject-$(PJPROJECT_VERSION).tar.bz2

pjproject: pjproject-$(PJPROJECT_VERSION).tar.bz2 .sum-pjproject
	$(UNPACK)
	$(APPLY) $(SRC)/pjproject/aconfigureupdate.patch
	$(APPLY) $(SRC)/pjproject/endianness.patch
	$(APPLY) $(SRC)/pjproject/unknowncipher.patch
	$(APPLY) $(SRC)/pjproject/gnutls.patch
	$(APPLY) $(SRC)/pjproject/notestsapps.patch
	$(APPLY) $(SRC)/pjproject/ipv6.patch
	$(APPLY) $(SRC)/pjproject/multiple_listeners.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.pjproject: pjproject
	cd $< && $(HOSTVARS) ./aconfigure $(HOSTCONF) $(PJPROJECT_OPTIONS)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
