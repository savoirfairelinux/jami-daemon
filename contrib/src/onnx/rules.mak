# ONNX
ONNX_VERSION := v1.6.0
ONNX_URL := https://github.com/microsoft/onnxruntime.git

$(TARBALLS)/onnxruntime-$(ONNX_VERSION).tar.xz:
	$(call download_git,$(ONNX_URL),$(ONNX_VERSION),$(ONNX_VERSION),preserve .git)

.sum-onnx: onnxruntime-$(ONNX_VERSION).tar.xz
	$(warning $@ not implemented)
	touch $@

onnx: onnxruntime-$(ONNX_VERSION).tar.xz .sum-onnx
	rm -Rf $@
	mkdir -p $@
	(cd $@ && tar x --strip-components=1 -f $<)

.onnx:  onnx
ifdef HAVE_ANDROID
	cd $< && sh build.sh --parallel --android --android_sdk_path $(ANDROID_SDK) --android_ndk_path $(ANDROID_NDK) --android_abi $(ANDROID_ABI) --android_api 29 --use_nnapi --config Release --build_shared_lib --skip_tests --android_cpp_shared --minimal_build extended
	cd $< && cp ./build/Linux/Release/libonnxruntime.so $(PREFIX)/lib/
else
ifdef HAVE_MACOSX
	cd $< && sh ./build.sh --config Release --build_shared_lib --parallel --skip_tests
	if [ ! -d "$(PREFIX)/lib/onnxruntime" ] ; then (mkdir $(PREFIX)/lib/onnxruntime) fi
	if [ ! -d "$(PREFIX)/lib/onnxruntime/cpu" ] ; then (mkdir $(PREFIX)/lib/onnxruntime/cpu) fi
	cd $< && cp ./build/MacOS/Release/libonnxruntime.dylib $(PREFIX)/lib/onnxruntime/cpu/libonnxruntime.dylib
else
ifdef USE_NVIDIA
	cd $< && sh ./build.sh --config Release --build_shared_lib --parallel --use_cuda --cuda_version $(CUDA_VERSION) --cuda_home $(CUDA_PATH) --cudnn_home $(CUDNN_PATH) --skip_tests
	if [ ! -d "$(PREFIX)/lib/onnxruntime" ] ; then (mkdir $(PREFIX)/lib/onnxruntime) fi
	if [ ! -d "$(PREFIX)/lib/onnxruntime/nvidia-gpu" ] ; then (mkdir $(PREFIX)/lib/onnxruntime/nvidia-gpu) fi
	cd $< && cp ./build/Linux/Release/libonnxruntime.so $(PREFIX)/lib/onnxruntime/nvidia-gpu/libonnxruntime.so
else
	cd $< && sh ./build.sh --config Release --build_shared_lib --parallel --skip_tests
	if [ ! -d "$(PREFIX)/lib/onnxruntime" ] ; then (mkdir $(PREFIX)/lib/onnxruntime) fi
	if [ ! -d "$(PREFIX)/lib/onnxruntime/cpu" ] ; then (mkdir $(PREFIX)/lib/onnxruntime/cpu) fi
	cd $< && cp ./build/Linux/Release/libonnxruntime.so $(PREFIX)/lib/onnxruntime/cpu/libonnxruntime.so
endif
endif
endif
	if [ ! -d "$(PREFIX)/include/onnxruntime" ] ; then ( mkdir $(PREFIX)/include/onnxruntime ) fi
	cd $< && cp -r ./include/onnxruntime/core/* $(PREFIX)/include/onnxruntime/
	touch $@
