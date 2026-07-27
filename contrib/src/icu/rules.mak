# ICU4C
ICU_VERSION := 78.3
PKG_CPE += cpe:2.3:a:unicode:icu:$(ICU_VERSION):*:*:*:*:*:*:*
ICU_URL := https://github.com/unicode-org/icu/archive/refs/tags/release-$(ICU_VERSION).tar.gz

# ICU is a plugin-only dependency. Do not add it to PKGS: TranslateMessages
# explicitly builds it, links it, and bundles its runtime libraries in the JPL.

# ICU requires a separate host build for cross-compilation. On native Linux,
# contrib's canonical target triple differs from the compiler's build triple
# despite producing runnable native binaries, so configure it as the build host.
ICU_CONF := --prefix="$(PREFIX)" \
	--datarootdir="$(PREFIX)/share" \
	--includedir="$(PREFIX)/include" \
	--libdir="$(PREFIX)/lib" \
	--program-prefix="" \
	--enable-shared --disable-static \
	--with-data-packaging=library \
	--disable-tests --disable-samples
ifdef HAVE_LINUX
ICU_CONF += --build="$(BUILD)" --host="$(BUILD)" --target="$(BUILD)"
else
ICU_CONF += $(HOSTCONF)
endif

$(TARBALLS)/icu-release-$(ICU_VERSION).tar.gz:
	$(call download,$(ICU_URL))

.sum-icu: icu-release-$(ICU_VERSION).tar.gz

icu: icu-release-$(ICU_VERSION).tar.gz .sum-icu
	$(UNPACK)
	$(MOVE)

.icu: icu
	mkdir -p $</icu4c/build
	cd $</icu4c/build && $(HOSTVARS) ../source/configure $(ICU_CONF)
	cd $</icu4c/build && $(MAKE) install
	touch $@