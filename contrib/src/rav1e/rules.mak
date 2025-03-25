# rav1e
RAV1E_HASH := 75fcd60a807c6fbc0f3d275e43ddccacc36cf9c4
RAV1E_GITURL := https://github.com/xiph/rav1e.git

RAV1ECONFIG := --prefix="$(PREFIX)" \
			   --release

$(TARBALLS)/rav1e-$(RAV1E_HASH).tar.xz:
	$(call download_git,$(RAV1E_GITURL),master,$(RAV1E_HASH))

.sum-rav1e: rav1e-$(RAV1E_HASH).tar.xz
	$(warning $@ not implemented)
	touch $@

rav1e: rav1e-$(RAV1E_HASH).tar.xz .sum-rav1e
	rm -Rf $@-$(RAV1E_HASH)
	mkdir -p $@-$(RAV1E_HASH)
	(cd $@-$(RAV1E_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f $<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.rav1e: rav1e
	cd $< && cargo cinstall $(RAV1ECONFIG)
ifdef HAVE_LINUX
	rm $(PREFIX)/lib/librav1e.so*
endif
	touch $@
