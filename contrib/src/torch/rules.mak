# TORCH
TORCH_HASH := 6c8c5ad5eaf47a62fafbb4a2747198cbffbf1ff0
TORCH_URL := https://github.com/pytorch/pytorch.git


download_torch_git = $(FLOCK_PREFIX) sh -c "\
  rm -Rf '$(@:.tar.xz=)' && \
  $(GIT) clone $(2:%=--branch '%') '$(1)' '$(@:.tar.xz=)' && \
  (cd '$(@:.tar.xz=)' && $(GIT) checkout $(3:%= '%')) || true && \
  (cd '$(dir $@)' && tar cJ '$(notdir $(@:.tar.xz=))') > '$@' && \
  rm -Rf '$(@:.tar.xz=)'"

# COMMAND to get the number of processor
NPROC := $(shell nproc)
PYTHON := which python3

TORCH_CMAKECONF := \
	  -DUSE_CUDA=OFF \
      -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_CUSTOM_PROTOBUF=ON \
      -DUSE_FBGEMM=ON \
      -DUSE_NNPACK=ON \
	  -DUSE_ONNX=ON \
	  -DBUILD_TEST=OFF \
      -DONNX_NAMESPACE="onnx" \
	  -DBUILD_PYTHON=ON \
	  -DCMAKE_JOB_POOL_COMPILE:STRING=$(NPROC) \
      -DUSE_QNNPACK=OFF -DUSE_THREAD=OFF -DUSE_PYTORCH_QNNPACK=OFF  -DUSE_XNNPACK=OFF -DUSE_DISTRIBUTED=OFF \
      -DUSE_MKLDNN=OFF -DBUILD_CAFFE2=ON  -DUSE_NCCL=OFF \
      -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX:PATH=pytorch-install

$(TARBALLS)/torch-$(TORCH_HASH).tar.xz:
	$(call download_torch_git,$(TORCH_URL),main,$(TORCH_HASH))

.sum-torch: torch-$(TORCH_HASH).tar.xz

torch: torch-$(TORCH_HASH).tar.xz
	$(UNPACK) && \
	mv torch-${TORCH_HASH} $@ && \
	cd $@ && \
	git submodule sync && git submodule update --init --recursive && \
	pip3 install -r requirements.txt && \
	mkdir -p pytorch-build && cd pytorch-build && \
	$(CMAKE) $(TORCH_CMAKECONF) .. && \
	make -j$(NPROC) && \
	touch $@

.torch: torch
	# check if all the files needed correctly installed
	cd $<
	mv pytorch-build/sleef/lib/* ../../lib/
	cp -r aten/src $(PREFIX)/include/ATen
	cp -R pytorch-build/aten/src/ATen/* $(PREFIX)/include/ATen
	cp -r c10 $(PREFIX)/include/c10
	cp -r caffe2 $(PREFIX)/include/caffe2
	cp -R pytorch-build/aten/src/caffe2/* $(PREFIX)/include/caffe2
	cp -R pytorch-build/c10/* $(PREFIX)/include/c10
	cp -R pytorch-build/lib/* $(PREFIX)/lib
	cp -r torch $(PREFIX)/include/torch &&
	touch $@
