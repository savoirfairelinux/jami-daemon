# TORCH
TORCH_VERSION := 2.2.0
TORCH_URL := https://github.com/pytorch/pytorch/archive/refs/tags/v$(TORCH_VERSION).tar.gz
PYTHON_VERSION := $(shell python3 --version 2>&1 | grep -oP '(?<=Python )(\d+\.\d+\.\d+)')

export CMAKE_CXX_STANDARD = 17
export CMAKE_CXX_STANDARD_REQUIRED = ON
export CMAKE_INSTALL_LIBDIR = lib
export BUILD_SHARED_LIBS = no
export CMAKE_BUILD_TYPE = Release
export CMAKE_JOB_POOL_COMPILE = 20

$(TARBALLS)/torch-$(TORCH_VERSION).tar.gz:
	$(call download,$(TORCH_URL))

.sum-torch: torch-$(TORCH_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

check-dependencies:
	$(if $(PYTHON_VERSION),,$(error Python 3.8.1 or higher is required))

build-dependencies: check-dependencies
	$(shell pip install -r torch/requirements.txt)
	$(GIT) submodule sync
	$(GIT) submodule update --init --recursive

torch: torch-$(TORCH_VERSION).tar.gz
	$(UNPACK)
	mv pytorch-$(TORCH_VERSION) $@ && touch $@

.torch: torch build-dependencies .sum-torch
	cd $< && cp -r * $(PREFIX)/
	cd $< && python3 setup.py develop --cmake
	touch $@
