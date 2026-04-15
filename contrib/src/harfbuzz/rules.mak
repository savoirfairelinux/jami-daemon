# harfbuzz
HARFBUZZ_VERSION := 8.4.0
HARFBUZZ_URL := https://github.com/harfbuzz/harfbuzz/releases/download/$(HARFBUZZ_VERSION)/harfbuzz-$(HARFBUZZ_VERSION).tar.xz

PKG_CPE += cpe:2.3:a:harfbuzz:harfbuzz:$(HARFBUZZ_VERSION):*:*:*:*:*:*:*

ifeq ($(call need_pkg,"harfbuzz >= $(HARFBUZZ_VERSION)"),)
PKGS_FOUND += harfbuzz
endif

HARFBUZZ_CONF := --prefix="$(PREFIX)" \
	--default-library=static \
	-Dtests=disabled \
	-Ddocs=disabled \
	-Dcairo=disabled \
	-Dicu=disabled \
	-Dintrospection=disabled

$(TARBALLS)/harfbuzz-$(HARFBUZZ_VERSION).tar.xz:
	$(call download,$(HARFBUZZ_URL))

.sum-harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz

harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz .sum-harfbuzz
	$(UNPACK)
	$(MOVE)

.harfbuzz: harfbuzz
	cd $< && $(HOSTVARS) meson setup build $(HARFBUZZ_CONF)
	cd $< && meson compile -C build
	cd $< && meson install -C build
	touch $@
