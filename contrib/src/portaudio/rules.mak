# PortAudio

PORTAUDIO_VERSION := v190700_20210406
PKG_CPE += cpe:2.3:a:*:portaudio:19.7.0:*:*:*:*:*:*:*
PORTAUDIO_URL := http://www.portaudio.com/archives/pa_stable_$(PORTAUDIO_VERSION).tgz

ifdef HAVE_WIN32
PKGS += portaudio
endif

ifeq ($(call need_pkg,"portaudio >= 2.0"),)
PKGS_FOUND += portaudio
endif

PORTAUDIOCONF := --with-winapi=directx

$(TARBALLS)/portaudio-$(PORTAUDIO_VERSION).tgz:
	$(call download,$(PORTAUDIO_URL))

.sum-portaudio: portaudio-$(PORTAUDIO_VERSION).tgz

portaudio: portaudio-$(PORTAUDIO_VERSION).tgz .sum-portaudio
	$(UNPACK)

.portaudio: portaudio
	$(APPLY) $(SRC)/portaudio/dsound_utf8.patch
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(PORTAUDIOCONF)
	cd $< && $(MAKE) install
	touch $@
