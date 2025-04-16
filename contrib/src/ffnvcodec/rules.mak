# ffnvcodec
FFNVCODEC_VERSION := n11.1.5.2
PKG_CPE += cpe:2.3:a:videolan:ffnvcodec:11.1.5.2:*:*:*:*:*:*:*
FFNVCODEC_URL := https://github.com/FFmpeg/nv-codec-headers/archive/$(FFNVCODEC_VERSION).tar.gz

ifeq ($(call need_pkg,"ffnvcodec >= 8"),)
PKGS_FOUND += ffnvcodec
endif

$(TARBALLS)/ffnvcodec-$(FFNVCODEC_VERSION).tar.gz:
	$(call download,$(FFNVCODEC_URL))

.sum-ffnvcodec: ffnvcodec-$(FFNVCODEC_VERSION).tar.gz

ffnvcodec: ffnvcodec-$(FFNVCODEC_VERSION).tar.gz .sum-ffnvcodec
	$(UNPACK)
	mv nv-codec-headers-* ffnvcodec-$(FFNVCODEC_VERSION)
	$(MOVE)

.ffnvcodec: ffnvcodec
	cd $< && $(HOSTVARS) DESTDIR=$(PREFIX) $(MAKE) install PREFIX=""
	touch $@
