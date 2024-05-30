# freetype
FREETYPE_HASH := 39ce3ac499d4cd7371031a062f410953c8ecce29
FREETYPE_GITURL := https://gitlab.freedesktop.org/freetype/freetype/-/archive/$(FREETYPE_HASH)/freetype-$(FREETYPE_HASH).tar.gz

ifeq ($(call need_pkg,"freetype2 >= 2.10.1"),)
PKGS_FOUND += freetype
endif

FTCONFIG := --build="$(BUILD)"   \
            --host="$(HOST)"     \
            --prefix="$(PREFIX)" \
            --enable-shared=no   \
            --enable-static=yes  \
            --with-zlib=no       \
            --with-png=no        \
            --with-harfbuzz=no

$(TARBALLS)/freetype-$(FREETYPE_HASH).tar.gz:
	$(call download,$(FREETYPE_GITURL))

.sum-freetype: freetype-$(FREETYPE_HASH).tar.gz

freetype: freetype-$(FREETYPE_HASH).tar.gz .sum-freetype
	rm -Rf $@-$(FREETYPE_HASH)
	mkdir -p $@-$(FREETYPE_HASH)
	(cd $@-$(FREETYPE_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f $<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.freetype: freetype
	cd $< && $(HOSTVARS) sh autogen.sh
	cd $< && $(HOSTVARS) ./configure $(FTCONFIG)
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@
