# lame
LAME_HASH := f416c19b3140a8610507ebb60ac7cd06e94472b8
LAME_GITURL := https://github.com/gypified/libmp3lame.git

LAMECONFIG := --prefix="$(PREFIX)"

$(TARBALLS)/mp3lame-$(LAME_HASH).tar.xz:
	$(call download_git,$(LAME_GITURL),master,$(LAME_HASH))

.sum-mp3lame: mp3lame-$(LAME_HASH).tar.xz
	$(warning $@ not implemented)
	touch $@

mp3lame: mp3lame-$(LAME_HASH).tar.xz .sum-mp3lame
	rm -Rf $@-$(LAME_HASH)
	mkdir -p $@-$(LAME_HASH)
	(cd $@-$(LAME_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f $<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.mp3lame: mp3lame
	cd $< && $(HOSTVARS) ./configure $(LAMECONFIG)
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@
