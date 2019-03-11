# ffnvcodec
FFNVCODEC_VERSION := n9.0.18.0
FFNVCODEC_GITURL := https://git.videolan.org/git/ffmpeg/nv-codec-headers.git

PKGS += ffnvcodec

ifeq ($(call need_pkg,"ffnvcodec >= 8"),)
PKGS_FOUND += ffnvcodec
endif

$(TARBALLS)/ffnvcodec-$(FFNVCODEC_VERSION).tar.xz:
	$(call download_git,$(FFNVCODEC_GITURL),master,$(FFNVCODEC_VERSION))

.sum-ffnvcodec: ffnvcodec-$(FFNVCODEC_VERSION).tar.xz
	$(warning $@ not implemented)
	touch $@

.sum-ffnvcodec: ffnvcodec-$(FFNVCODEC_VERSION).tar.xz

ffnvcodec: ffnvcodec-$(FFNVCODEC_VERSION).tar.xz .sum-ffnvcodec
	$(UNPACK)
	$(MOVE)

.ffnvcodec: ffnvcodec
	cd $< && $(HOSTVARS) DESTDIR=$(PREFIX) $(MAKE) install PREFIX=""
	touch $@
