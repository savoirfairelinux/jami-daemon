# webrtc-audio-processing

WEBRTCAP_VER := d0569cfa50c1858ee279d77b3fc8870be6902441
WEBRTCAP_URL := https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing/-/archive/$(WEBRTCAP_VER)/webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz

ifndef HAVE_DARWIN_OS
PKGS += webrtc-audio-processing
endif
ifeq ($(call need_pkg,"webrtc-audio-processing >= 2.0"),)
PKGS_FOUND += webrtc-audio-processing
endif

$(TARBALLS)/webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz:
	$(call download,$(WEBRTCAP_URL))

.sum-webrtc-audio-processing: webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz

webrtc-audio-processing: webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz .sum-webrtc-audio-processing
	$(UNPACK)
	$(MOVE)

.webrtc-audio-processing: webrtc-audio-processing
	cd $< && $(HOSTVARS) meson setup build --prefix="$(PREFIX)" --libdir="$(PREFIX)/lib"
	cd $< && meson compile -C build
	cd $< && meson install -C build
	touch $@
