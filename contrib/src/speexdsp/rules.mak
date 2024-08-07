# speexdsp

SPEEXDSP_VERSION := 1.2.0
SPEEXDSP_HASH := SpeexDSP-$(SPEEXDSP_VERSION)
PKG_CPE += cpe:2.3:a:xiph:speex:$(SPEEXDSP_VERSION):*:*:*:*:*:*:*
SPEEXDSP_GITURL := https://gitlab.xiph.org/xiph/speexdsp/-/archive/$(SPEEXDSP_HASH)/speexdsp-$(SPEEXDSP_HASH).tar.gz

PKGS += speexdsp
ifeq ($(call need_pkg,"speexdsp"),)
PKGS_FOUND += speexdsp
endif

$(TARBALLS)/speexdsp-$(SPEEXDSP_HASH).tar.gz:
	$(call download,$(SPEEXDSP_GITURL))

.sum-speexdsp: speexdsp-$(SPEEXDSP_HASH).tar.gz

speexdsp: speexdsp-$(SPEEXDSP_HASH).tar.gz
	rm -Rf $@ $@-$(SPEEXDSP_HASH)
	mkdir -p $@-$(SPEEXDSP_HASH)
	$(ZCAT) "$<" | (cd $@-$(SPEEXDSP_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1)
	$(MOVE)

SPEEXDSP_CONF := --enable-resample-full-sinc-table --disable-examples
ifeq ($(ARCH),aarch64)
# old neon, not compatible with aarch64
SPEEXDSP_CONF += --disable-neon
endif
ifeq ($(ARCH),arm64)
# old neon, not compatible with arm64(==aarch64)
SPEEXDSP_CONF += --disable-neon
endif
ifndef HAVE_NEON
SPEEXDSP_CONF += --disable-neon
endif
ifndef HAVE_FPU
SPEEXDSP_CONF += --enable-fixed-point
ifeq ($(ARCH),arm)
SPEEXDSP_CONF += --enable-arm5e-asm
endif
endif

.speexdsp: speexdsp .sum-speexdsp
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(SPEEXDSP_CONF)
	cd $< && $(MAKE) install
	touch $@
