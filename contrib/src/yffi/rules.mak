# YFFI (Y-CRDT "libyrs") — Rust CRDT engine used for real-time collaborative editing.
# Provides the C FFI (libyrs.a + libyrs.h) consumed by the daemon collaborative module.
PKGS += yffi
YFFI_VERSION := 03e14a0232903498299a9e717c7ee8001e40e5db
YFFI_URL := https://github.com/y-crdt/y-crdt/archive/$(YFFI_VERSION).tar.gz

# Rust's cargo is a host build tool (like a compiler). Prefer a cargo already on
# the build host (e.g. the GNU/Linux packaging image ships the distro toolchain);
# otherwise bootstrap a pinned toolchain with rustup into a contrib-local
# directory, so CI images that lack Rust (Android, iOS, ...) can still build the
# crate. A pre-seeded system cargo keeps offline builds working unchanged.
YFFI_RUST_VERSION := 1.96.0
YFFI_RUST_DIR := $(abspath rust)
YFFI_SYSTEM_CARGO := $(shell command -v cargo 2>/dev/null)
ifeq ($(YFFI_SYSTEM_CARGO),)
CARGO := $(YFFI_RUST_DIR)/cargo/bin/cargo
YFFI_RUSTUP := $(YFFI_RUST_DIR)/cargo/bin/rustup
YFFI_RUST_ENV := RUSTUP_HOME="$(YFFI_RUST_DIR)/rustup" CARGO_HOME="$(YFFI_RUST_DIR)/cargo"
else
CARGO ?= $(YFFI_SYSTEM_CARGO)
YFFI_RUSTUP := $(shell command -v rustup 2>/dev/null)
YFFI_RUST_ENV :=
endif

ifeq ($(call need_pkg,'yrs >= 0.27'),)
PKGS_FOUND += yffi
endif

# Cross-compilation for Android: map the contrib host triple to the matching Rust
# target triple and point cargo at the NDK clang as linker. The contrib host triple
# uses "armv7a" while Rust uses "armv7"; everything else matches.
ifdef HAVE_ANDROID
YFFI_RUST_TARGET := $(patsubst armv7a-%,armv7-%,$(HOST))
YFFI_NDK_API := $(patsubst android-%,%,$(ANDROID_API))
YFFI_NDK_BIN := $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin
YFFI_NDK_CLANG := $(YFFI_NDK_BIN)/$(HOST)$(YFFI_NDK_API)-clang
YFFI_CARGO_LINKER_VAR := CARGO_TARGET_$(shell echo $(YFFI_RUST_TARGET) | tr 'a-z-' 'A-Z_')_LINKER
YFFI_CARGO_ENV := $(YFFI_CARGO_LINKER_VAR)="$(YFFI_NDK_CLANG)" \
	CC="$(YFFI_NDK_CLANG)" AR="$(YFFI_NDK_BIN)/llvm-ar"
YFFI_CARGO_TARGET_ARG := --target $(YFFI_RUST_TARGET)
YFFI_LIB_SUBDIR := $(YFFI_RUST_TARGET)/release
# Android's libc provides pthread; there is no separate -lpthread to link.
YFFI_PRIVATE_LIBS := -ldl -lm
else
YFFI_CARGO_ENV :=
YFFI_CARGO_TARGET_ARG :=
YFFI_LIB_SUBDIR := release
YFFI_PRIVATE_LIBS := -lpthread -ldl -lm
endif

$(TARBALLS)/yffi-$(YFFI_VERSION).tar.gz:
	$(call download,$(YFFI_URL))

.sum-yffi: yffi-$(YFFI_VERSION).tar.gz

yffi: yffi-$(YFFI_VERSION).tar.gz
	$(UNPACK)
	mv y-crdt-$(YFFI_VERSION) yffi-$(YFFI_VERSION)
	$(APPLY) $(SRC)/yffi/0001-avoid-if-let-guard.patch
	$(MOVE)

# Bootstrap a pinned Rust toolchain via rustup when the build host has no cargo.
$(YFFI_RUST_DIR)/cargo/bin/cargo:
	mkdir -p "$(YFFI_RUST_DIR)"
	$(YFFI_RUST_ENV) sh -c "curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
		sh -s -- -y --no-modify-path --profile minimal \
		--default-toolchain $(YFFI_RUST_VERSION)"

# Builds the static library for the active (native or Android cross) target. Only
# the static lib (libyrs.a) is installed; the C header is shipped with the source.
.yffi: yffi .sum-yffi $(CARGO)
ifneq ($(YFFI_CARGO_TARGET_ARG),)
	$(YFFI_RUST_ENV) $(YFFI_RUSTUP) target add $(YFFI_RUST_TARGET)
endif
	cd $< && $(YFFI_RUST_ENV) $(YFFI_CARGO_ENV) $(CARGO) build --release --manifest-path yffi/Cargo.toml \
		--target-dir target $(YFFI_CARGO_TARGET_ARG)
	mkdir -p "$(PREFIX)/lib/pkgconfig" "$(PREFIX)/include"
	install -m644 $</target/$(YFFI_LIB_SUBDIR)/libyrs.a "$(PREFIX)/lib/libyrs.a"
	install -m644 $</tests-ffi/include/libyrs.h "$(PREFIX)/include/libyrs.h"
	sed -e 's|@PREFIX@|$(PREFIX)|g' -e 's|@VERSION@|0.27.2|g' -e 's|@PRIVATE_LIBS@|$(YFFI_PRIVATE_LIBS)|g' \
		$(SRC)/yffi/yrs.pc.in > "$(PREFIX)/lib/pkgconfig/yrs.pc"
	touch $@
