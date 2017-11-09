# speex

SPEEX_HASH := 9172c7ef85fbf058027528d48ededbc7ca847908
SPEEX_GITURL := https://git.xiph.org/?p=speex.git;a=snapshot;h=$(SPEEX_HASH);sf=tgz

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
ifeq ($(ARCH),aarch64)
SPEEX_CONF += --disable-neon
endif

.speex: speex .sum-speex
	mkdir -p $</m4 && $(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(SPEEX_CONF)
	cd $< && $(MAKE) install
	touch $@
