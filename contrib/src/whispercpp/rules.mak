# whispercpp
WHISPERCPP_HASH := v1.2.1
WHISPERCPP_GITURL := https://github.com/ggerganov/whisper.cpp.git

WCONFIG := -DBUILD_SHARED_LIBS=OFF \
		   -DCMAKE_POSITION_INDEPENDENT_CODE=ON

ifdef HAVE_MACOSX
WCONFIG += -DCMAKE_OSX_ARCHITECTURES=${ARCH}
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
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.whispercpp: whispercpp
	cd $< && cmake . $(WCONFIG)
	cd $< && $(MAKE)
	cd $< && cp libwhisper.a $(PREFIX)/lib
	cd $< && cp whisper.h $(PREFIX)/include
	touch $@
