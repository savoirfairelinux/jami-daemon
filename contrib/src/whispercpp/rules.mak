# whispercpp
WHISPERCPP_HASH := v1.8.4
WHISPERCPP_GITURL := https://github.com/ggerganov/whisper.cpp.git

WHISPER_CMAKECONF := -DBUILD_SHARED_LIBS=OFF \
		   -DWHISPER_BUILD_EXAMPLES=OFF \
		   -DWHISPER_BUILD_TESTS=OFF \
		   -DWHISPER_BUILD_SERVER=OFF \
		   -DGGML_STATIC=ON \
		   -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
		   -DGGML_BLAS=0 \
		   -DGGML_CUDA=0\

ifdef HAVE_MACOSX
WHISPER_CMAKECONF += -DCMAKE_OSX_ARCHITECTURES=${ARCH}
endif

# Determine which libraries to link based on BLAS and CUDA settings
WHISPER_LIBS_BASE := -Wl,--whole-archive $${libdir}/libwhisper.a $${libdir}/libggml.a $${libdir}/libggml-base.a $${libdir}/libggml-cpu.a -Wl,--no-whole-archive

ifeq ($(findstring -DGGML_BLAS=1,$(WHISPER_CMAKECONF)),)
# BLAS disabled
else
WHISPER_LIBS_BASE += -lopenblas
endif

ifeq ($(findstring -DGGML_CUDA=1,$(WHISPER_CMAKECONF)),)
# CUDA disabled
else
WHISPER_LIBS_BASE += -lcuda -lcudart -lcublas
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
	sed -i 's|^Libs:.*|Libs: -L$${libdir} $(WHISPER_LIBS_BASE) -fopenmp|' $(PREFIX)/lib/pkgconfig/whisper.pc
	touch $@