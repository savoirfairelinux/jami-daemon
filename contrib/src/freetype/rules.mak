# freetype
FREETYPE_HASH := 39ce3ac499d4cd7371031a062f410953c8ecce29
FREETYPE_GITURL := https://gitlab.freedesktop.org/freetype/freetype.git

FTCONFIG := --build="$(BUILD)"   \
            --host="$(HOST)"     \
            --prefix="$(PREFIX)" \
            --enable-shared=no   \
            --enable-static=yes  \
            --with-zlib=no       \
            --with-png=no        \
            --with-harfbuzz=no

$(TARBALLS)/freetype-$(FREETYPE_HASH).tar.xz:
	$(call download_git,$(FREETYPE_GITURL),master,$(FREETYPE_HASH))

.sum-freetype: freetype-$(FREETYPE_HASH).tar.xz
	$(warning $@ not implemented)
	touch $@

freetype: freetype-$(FREETYPE_HASH).tar.xz .sum-freetype
	rm -Rf $@-$(FREETYPE_HASH)
	mkdir -p $@-$(FREETYPE_HASH)
	(cd $@-$(FREETYPE_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f $<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.freetype: freetype
	cd $< && cp ./../../src/freetype/autogen.sh .
ifdef HAVE_ANDROID
	cd $< && $(HOSTVARS) sh autogen.sh
	cd $< && $(HOSTVARS) ./configure $(FTCONFIG)
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
endif
	touch $@
