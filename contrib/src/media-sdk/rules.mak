# MEDIA-SDK
MEDIA_SDK_HASH := intel-mediasdk-19.2.0
MEDIA_SDK_URL := https://github.com/Intel-Media-SDK/MediaSDK/archive/$(MEDIA_SDK_HASH).tar.gz

ifdef HAVE_LINUX
ifndef HAVE_ANDROID

ifeq ($(call need_pkg,"libmfx >= 1.26"),)
PKGS_FOUND += media-sdk
endif

MEDIA_SDK_CMAKECONF := -DCMAKE_INSTALL_LIBDIR=lib \
                       -DCMAKE_INSTALL_PREFIX=${PREFIX} \

$(TARBALLS)/media-sdk-$(MEDIA_SDK_HASH).tar.gz:
	$(call download,$(MEDIA_SDK_URL))

.sum-media-sdk: media-sdk-$(MEDIA_SDK_HASH).tar.gz

media-sdk: media-sdk-$(MEDIA_SDK_HASH).tar.gz .sum-media-sdk
	rm -Rf $@ $@-$(MEDIA_SDK_HASH)
	mkdir -p $@-$(MEDIA_SDK_HASH)
	(cd $@-$(MEDIA_SDK_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f ../$<)
	$(MOVE)

.media-sdk: media-sdk
	$(APPLY) $(SRC)/media-sdk/linux-static-lib-build.patch
	cd $< && cd api/mfx_dispatch/linux && mkdir -p build
	cd $< && cd api/mfx_dispatch/linux/build && $(HOSTVARS) cmake .. $(MEDIA_SDK_CMAKECONF)
	cd $< && cd api/mfx_dispatch/linux/build && $(MAKE) install
	touch $@

endif
endif