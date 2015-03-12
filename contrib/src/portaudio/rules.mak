# PortAudio

PORTAUDIO_VERSION := v19_20140130
PORTAUDIO_URL := http://www.portaudio.com/archives/pa_stable_$(PORTAUDIO_VERSION).tgz

PKGS += portaudio

ifeq ($(call need_pkg,"portaudio >= 2.0"),)
PKGS_FOUND += portaudio
endif


$(TARBALLS)/portaudio-$(PORTAUDIO_VERSION).tgz:
	$(call download,$(PORTAUDIO_URL))

.sum-portaudio: portaudio-$(PORTAUDIO_VERSION).tgz

portaudio: portaudio-$(PORTAUDIO_VERSION).tgz .sum-portaudio
	$(UNPACK)

.portaudio: portaudio
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
