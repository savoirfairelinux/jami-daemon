# webrtc-audio-processing

WEBRTCAP_VER := v2.1
WEBRTCAP_URL := https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing/-/archive/$(WEBRTCAP_VER)/webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz

ifndef HAVE_DARWIN_OS
PKGS += webrtc-audio-processing
endif
ifeq ($(call need_pkg,"webrtc-audio-processing-2 >= 2.0"),)
PKGS_FOUND += webrtc-audio-processing
endif

# abseil comes from the meson wrap (no contrib package); the wrap downloads it.
WEBRTCAP_CONF := --prefix="$(PREFIX)" \
	--default-library=static \
	--libdir=lib \
	-Dprefer_static=true

$(TARBALLS)/webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz:
	$(call download,$(WEBRTCAP_URL))

.sum-webrtc-audio-processing: webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz

webrtc-audio-processing: webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz .sum-webrtc-audio-processing
	$(UNPACK)
	$(APPLY) $(SRC)/webrtc-audio-processing/fix-gcc15-missing-cstdint.patch
	$(APPLY) $(SRC)/webrtc-audio-processing/install-vad-header.patch
	$(MOVE)

.webrtc-audio-processing: webrtc-audio-processing
	cd $< && $(HOSTVARS) meson setup build $(WEBRTCAP_CONF)
	cd $< && meson compile -C build
	cd $< && meson install -C build
	touch $@
