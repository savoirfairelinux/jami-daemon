# webrtc-audio-processing

WEBRTCAP_VER := v0.3.1
WEBRTCAP_URL := https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing/-/archive/$(WEBRTCAP_VER)/webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz

ifndef HAVE_DARWIN_OS
PKGS += webrtc-audio-processing
endif
ifeq ($(call need_pkg,"webrtc-audio-processing"),)
PKGS_FOUND += webrtc-audio-processing
endif

$(TARBALLS)/webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz:
	$(call download,$(WEBRTCAP_URL))

.sum-webrtc-audio-processing: webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz

webrtc-audio-processing: webrtc-audio-processing-$(WEBRTCAP_VER).tar.gz .sum-webrtc-audio-processing
	$(UNPACK)
	$(MOVE)

.webrtc-audio-processing: webrtc-audio-processing
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-neon=no
	cd $< && $(MAKE) install
	touch $@
