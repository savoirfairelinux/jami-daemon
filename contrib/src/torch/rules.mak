# TORCH
TORCH_VERSION := 2.2.0
TORCH_URL := https://github.com/pytorch/pytorch/archive/refs/tags/v$(TORCH_VERSION).tar.gz

TORCH_CMAKECONF := \
		-DCMAKE_CXX_STANDARD=17 \
		-DCMAKE_CXX_STANDARD_REQUIRED=ON \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DBUILD_SHARED_LIBS=no \
		-DCMAKE_BUILD_TYPE=Release \
    	-DCMAKE_JOB_POOL_COMPILE:STRING=20

$(TARBALLS)/torch-$(TORCH_VERSION).tar.gz:
	$(call download,$(TORCH_URL))

.sum-torch: torch-$(TORCH_VERSION).tar.gz

check-dependencies: .sum-torch
	PYTHON_VERSION := $(shell python3 --version 2>&1 | grep -oP '(?<=Python )(\d+\.\d+\.\d+)')
	$(if $(PYTHON_VERSION),,$(error Python 3.8.1 or higher is required))

build-dependencies: .sum-torch .check-dependencies
	$(shell pip install -r $@/requirements.txt)
	$(GIT) submodule sync
	$(GIT) submodule update --init --recursive

torch: torch-$(TORCH_VERSION).tar.gz
	$(UNPACK)
	mv pytorch-$(TORCH_VERSION) $@ && touch $@

.torch: torch build-dependencies
	cd $< && cp -r * $(PREFIX)/
	# TODO: add torch cmake configuration
	cd $< && python3 setup.py develop --cmake
	touch $@
