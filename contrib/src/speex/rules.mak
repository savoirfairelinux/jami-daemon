# speex
SPEEX_VERSION := 1.2.1
SPEEX_HASH := Speex-$(SPEEX_VERSION)
PKG_CPE += cpe:2.3:a:xiph:speex:$(SPEEX_VERSION):*:*:*:*:*:*:*
SPEEX_GITURL := https://gitlab.xiph.org/xiph/speex/-/archive/$(SPEEX_HASH)/speex-$(SPEEX_HASH).tar.gz

PKGS += speex
ifeq ($(call need_pkg,"speex >= 1.0.5"),)
PKGS_FOUND += speex
endif

$(TARBALLS)/speex-$(SPEEX_HASH).tar.gz:
	$(call download,$(SPEEX_GITURL))

.sum-speex: speex-$(SPEEX_HASH).tar.gz

speex: speex-$(SPEEX_HASH).tar.gz
	rm -Rf $@ $@-$(SPEEX_HASH)
	mkdir -p $@-$(SPEEX_HASH)
	$(ZCAT) "$<" | (cd $@-$(SPEEX_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1)
	$(MOVE)

SPEEX_CONF := --disable-binaries
ifndef HAVE_FPU
SPEEX_CONF += --enable-fixed-point
ifeq ($(ARCH),arm)
SPEEX_CONF += --enable-arm5e-asm
endif
endif

.speex: speex .sum-speex
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(SPEEX_CONF)
	cd $< && $(MAKE) install
	touch $@
