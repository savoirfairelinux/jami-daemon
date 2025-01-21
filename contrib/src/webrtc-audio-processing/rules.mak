# webrtc-audio-processing

WEBRTCAP_VER := v2.0
WEBRTCAP_URL := https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing/-/archive/$(WEBRTCAP_VER)/webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz

ifndef HAVE_DARWIN_OS
PKGS += webrtc-audio-processing
endif
ifeq ($(call need_pkg,"webrtc-audio-processing >= 0.3"),)
PKGS_FOUND += webrtc-audio-processing
endif

$(TARBALLS)/webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz:
	$(call download,$(WEBRTCAP_URL))

.sum-webrtc-audio-processing: webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz

webrtc-audio-processing: webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz .sum-webrtc-audio-processing
	$(UNPACK)
	$(MOVE)

.webrtc-audio-processing: webrtc-audio-processing
	cd $< && $(HOSTVARS) meson setup build --prefix="$(PREFIX)"
	cd $< && meson compile -C build
	cd $< && meson install -C build
	touch $@
