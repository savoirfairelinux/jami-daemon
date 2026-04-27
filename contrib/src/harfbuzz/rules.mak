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

ifneq ($(HOST),$(BUILD))
HARFBUZZ_CONF += --cross-file $(abspath harfbuzz-cross-file.ini)
endif

$(TARBALLS)/harfbuzz-$(HARFBUZZ_VERSION).tar.xz:
	$(call download,$(HARFBUZZ_URL))

.sum-harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz

harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz .sum-harfbuzz
	$(UNPACK)
	$(MOVE)

harfbuzz-cross-file.ini:
	ARCH=$$(echo $(HOST) | cut -d- -f1); \
	case "$$ARCH" in \
		aarch64) CPU_FAM=aarch64; CPU=aarch64 ;; \
		arm*) CPU_FAM=arm; CPU=armv7 ;; \
		x86_64) CPU_FAM=x86_64; CPU=x86_64 ;; \
		i686|i386) CPU_FAM=x86; CPU=i686 ;; \
		*) CPU_FAM=$$ARCH; CPU=$$ARCH ;; \
	esac; \
	SYS=$$(echo "$(HOST)" | grep -q android && echo android || echo linux); \
	printf "[host_machine]\nsystem = '$$SYS'\ncpu_family = '$$CPU_FAM'\ncpu = '$$CPU'\nendian = 'little'\n\n[properties]\nneeds_exe_wrapper = true\n" > $@

.harfbuzz: harfbuzz $(if $(filter-out $(BUILD),$(HOST)),harfbuzz-cross-file.ini)
	cd $< && $(HOSTVARS) meson setup build $(HARFBUZZ_CONF)
	cd $< && meson compile -C build
	cd $< && meson install -C build
	touch $@