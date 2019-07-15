# MEDIA-SDK
MEDIA_SDK_HASH := bf164fb6d7b906fc3d9bad89ac6079798d478efd
MEDIA_SDK_URL := https://github.com/Intel-Media-SDK/MediaSDK/archive/$(MEDIA_SDK_HASH).tar.gz

ifdef HAVE_LINUX
ifndef HAVE_ANDROID

PKGS+=mediasdk

ifeq ($(call need_pkg,"libmfx >= 1.26"),)
PKGS_FOUND += mediasdk
endif

DEPS_mediasdk = libva

$(TARBALLS)/mediasdk-$(MEDIA_SDK_HASH).tar.gz:
	$(call download,$(MEDIA_SDK_URL))

.sum-mediasdk: mediasdk-$(MEDIA_SDK_HASH).tar.gz

mediasdk: mediasdk-$(MEDIA_SDK_HASH).tar.gz
	rm -Rf $@ $@-$(MEDIA_SDK_HASH)
	mkdir -p $@-$(MEDIA_SDK_HASH)
	(cd $@-$(MEDIA_SDK_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f ../$<)
	$(MOVE)

.mediasdk: mediasdk 
	cd $< && $(HOSTVARS) mkdir -p build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr && $(HOSTVARS) $(MAKE)
	touch $@

endif
endif