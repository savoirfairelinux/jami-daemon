# whispercpp
WHISPERCPP_HASH := v1.7.5
WHISPERCPP_GITURL := https://github.com/ggerganov/whisper.cpp.git

WHISPER_CMAKECONF := -DBUILD_SHARED_LIBS=OFF \
		   -DWHISPER_BUILD_EXAMPLES=OFF \
		   -DWHISPER_BUILD_TESTS=OFF \
		   -DWHISPER_BUILD_SERVER=OFF \
		   -DGGML_STATIC=ON \
		   -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
		   -DGGML_BLAS=1 \
		   #-DGGML_STATIC=ON \

ifdef HAVE_MACOSX
WHISPER_CMAKECONF += -DCMAKE_OSX_ARCHITECTURES=${ARCH}
endif

$(TARBALLS)/whispercpp-$(WHISPERCPP_HASH).tar.xz:
	$(call download_git,$(WHISPERCPP_GITURL),master,$(WHISPERCPP_HASH))

.sum-whispercpp: whispercpp-$(WHISPERCPP_HASH).tar.xz
	$(warning $@ not implemented)
	touch $@

whispercpp: whispercpp-$(WHISPERCPP_HASH).tar.xz .sum-whispercpp
	rm -Rf $@-$(WHISPERCPP_HASH)
	mkdir -p $@-$(WHISPERCPP_HASH)
	(cd $@-$(WHISPERCPP_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f $<)
	$(MOVE)

.whispercpp: whispercpp
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) .. $(WHISPER_CMAKECONF)
	cd $< && cd build && $(MAKE)
	cd $< && cd build && $(MAKE) install
	sed -i 's|^Libs:.*|Libs: -L$$\{libdir\} -lwhisper -lggml -lggml-base -lggml-cpu -lggml-blas -lopenblas -fopenmp|' $(PREFIX)/lib/pkgconfig/whisper.pc
	touch $@
