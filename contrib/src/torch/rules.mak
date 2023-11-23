# TORCH
TORCH_VERSION := latest
TORCH_URL := https://download.pytorch.org/libtorch/nightly/cpu/libtorch-macos-arm64-$(TORCH_VERSION).zip

TORCH_CMAKECONF := \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DBUILD_SHARED_LIBS=no \
		-DCMAKE_BUILD_TYPE=Release \
    	-DCMAKE_JOB_POOL_COMPILE:STRING=20

$(TARBALLS)/torch-$(TORCH_VERSION).zip:
	$(call download,$(TORCH_URL))

.sum-torch: torch-$(TORCH_VERSION).zip

torch: torch-$(TORCH_VERSION).zip
	$(UNPACK)
	mv libtorch $@ && touch $@

.torch: torch
	cd $< && cp -r * $(PREFIX)/
	touch $@
