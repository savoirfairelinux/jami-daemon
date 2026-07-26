# YFFI (Y-CRDT "libyrs") — Rust CRDT engine used for real-time collaborative editing.
# Provides the C FFI (libyrs.a + libyrs.h) consumed by the daemon collaborative module.
PKGS += yffi
YFFI_VERSION := 03e14a0232903498299a9e717c7ee8001e40e5db
YFFI_URL := https://github.com/y-crdt/y-crdt/archive/$(YFFI_VERSION).tar.gz

# Rust's cargo is a host build tool (like a compiler); allow override, fall back to rustup's path.
CARGO ?= $(shell command -v cargo 2>/dev/null || echo $(HOME)/.cargo/bin/cargo)
RUSTUP ?= $(shell command -v rustup 2>/dev/null || echo $(HOME)/.cargo/bin/rustup)

ifeq ($(call need_pkg,'yrs >= 0.27'),)
PKGS_FOUND += yffi
endif

# Unless told otherwise cargo builds for the machine running it, while contrib builds
# one prefix per target architecture (macOS then merges the prefixes into a universal
# binary). The Rust target triple therefore has to be derived from the contrib host for
# every platform we cross-build, and cargo spells some triples differently.
ifdef HAVE_ANDROID
# The contrib host triple uses "armv7a" while Rust uses "armv7"; everything else matches.
YFFI_RUST_TARGET := $(patsubst armv7a-%,armv7-%,$(HOST))
YFFI_NDK_API := $(patsubst android-%,%,$(ANDROID_API))
YFFI_NDK_BIN := $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin
YFFI_NDK_CLANG := $(YFFI_NDK_BIN)/$(HOST)$(YFFI_NDK_API)-clang
YFFI_CARGO_LINKER_VAR := CARGO_TARGET_$(shell echo $(YFFI_RUST_TARGET) | tr 'a-z-' 'A-Z_')_LINKER
YFFI_CARGO_ENV := $(YFFI_CARGO_LINKER_VAR)="$(YFFI_NDK_CLANG)" \
	CC="$(YFFI_NDK_CLANG)" AR="$(YFFI_NDK_BIN)/llvm-ar"
# Android's libc provides pthread; there is no separate -lpthread to link.
YFFI_PRIVATE_LIBS := -ldl -lm
else ifdef HAVE_IOS
ifeq ($(IOS_TARGET_PLATFORM),iPhoneOS)
YFFI_RUST_TARGET := aarch64-apple-ios
else
YFFI_RUST_TARGET := $(if $(filter arm64 aarch64,$(ARCH)),aarch64-apple-ios-sim,x86_64-apple-ios)
endif
YFFI_CARGO_ENV := IPHONEOS_DEPLOYMENT_TARGET="$(MIN_IOS_VERSION)"
YFFI_PRIVATE_LIBS := -lpthread -ldl -lm
else ifdef HAVE_MACOSX
# contrib says "arm64" (as Apple does) where Rust says "aarch64".
YFFI_RUST_TARGET := $(if $(filter arm64 aarch64,$(ARCH)),aarch64,$(ARCH))-apple-darwin
YFFI_CARGO_ENV := MACOSX_DEPLOYMENT_TARGET="$(MIN_OSX_VERSION)"
YFFI_PRIVATE_LIBS := -lpthread -ldl -lm
else
# Native build: let cargo pick its own default target.
YFFI_RUST_TARGET :=
YFFI_CARGO_ENV :=
YFFI_PRIVATE_LIBS := -lpthread -ldl -lm
endif

# Cargo nests its output under the triple as soon as --target is passed explicitly.
ifeq ($(YFFI_RUST_TARGET),)
YFFI_CARGO_TARGET_ARG :=
YFFI_LIB_SUBDIR := release
else
YFFI_CARGO_TARGET_ARG := --target $(YFFI_RUST_TARGET)
YFFI_LIB_SUBDIR := $(YFFI_RUST_TARGET)/release
endif

$(TARBALLS)/yffi-$(YFFI_VERSION).tar.gz:
	$(call download,$(YFFI_URL))

.sum-yffi: yffi-$(YFFI_VERSION).tar.gz

yffi: yffi-$(YFFI_VERSION).tar.gz
	$(UNPACK)
	mv y-crdt-$(YFFI_VERSION) yffi-$(YFFI_VERSION)
	$(APPLY) $(SRC)/yffi/0001-avoid-if-let-guard.patch
	$(MOVE)

# Builds the static library for the active target (native, or cross for Android, iOS
# and the non-native macOS slice). Only the static lib (libyrs.a) is installed; the C
# header is shipped with the source.
.yffi: yffi .sum-yffi
	@command -v $(CARGO) >/dev/null 2>&1 || { \
		echo "yffi: cargo not found. Install the Rust toolchain or set CARGO=/path/to/cargo."; \
		exit 1; \
	}
# Rust ships the standard library of the host target only, so a cross target has to be
# added first. Best effort: toolchains not managed by rustup either already provide the
# target or fail with a clearer message from cargo below.
	@if [ -n "$(YFFI_RUST_TARGET)" ] && command -v $(RUSTUP) >/dev/null 2>&1; then \
		$(RUSTUP) target add $(YFFI_RUST_TARGET) || true; \
	fi
	cd $< && $(YFFI_CARGO_ENV) $(CARGO) build --release --manifest-path yffi/Cargo.toml \
		--target-dir target $(YFFI_CARGO_TARGET_ARG)
	mkdir -p "$(PREFIX)/lib/pkgconfig" "$(PREFIX)/include"
	install -m644 $</target/$(YFFI_LIB_SUBDIR)/libyrs.a "$(PREFIX)/lib/libyrs.a"
	install -m644 $</tests-ffi/include/libyrs.h "$(PREFIX)/include/libyrs.h"
	sed -e 's|@PREFIX@|$(PREFIX)|g' -e 's|@VERSION@|0.27.2|g' -e 's|@PRIVATE_LIBS@|$(YFFI_PRIVATE_LIBS)|g' \
		$(SRC)/yffi/yrs.pc.in > "$(PREFIX)/lib/pkgconfig/yrs.pc"
	touch $@
