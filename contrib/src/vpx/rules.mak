# libvpx
VPX_VERSION := 1.14.1
VPX_HASH := v$(VPX_VERSION)
PKG_CPE += cpe:2.3:a:webmproject:libvpx:$(VPX_VERSION):*:*:*:*:*:*:*
VPX_URL := https://github.com/webmproject/libvpx/archive/$(VPX_HASH).tar.gz

$(TARBALLS)/libvpx-$(VPX_HASH).tar.gz:
	$(call download,$(VPX_URL))

.sum-vpx: libvpx-$(VPX_HASH).tar.gz

libvpx: libvpx-$(VPX_HASH).tar.gz .sum-vpx
	rm -Rf $@-$(VPX_HASH) $@
	mkdir -p $@-$(VPX_HASH)
	(cd $@-$(VPX_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f $<)
	$(MOVE)

ifeq ($(call need_pkg,"vpx >= 1.0"),)
PKGS_FOUND += vpx
endif
DEPS_vpx =

ifdef HAVE_CROSS_COMPILE
ifndef HAVE_IOS
ifndef HAVE_MACOSX
VPX_CROSS := $(CROSS_COMPILE)
endif
endif
else
VPX_CROSS :=
endif


ifeq ($(ARCH),arm)
VPX_ARCH := armv7
else ifeq ($(ARCH),arm64)
VPX_ARCH := arm64
else ifeq ($(ARCH),i386)
VPX_ARCH := x86
else ifeq ($(ARCH),mips)
VPX_ARCH := mips32
else ifeq ($(ARCH),ppc)
VPX_ARCH := ppc32
else ifeq ($(ARCH),ppc64)
VPX_ARCH := ppc64
else ifeq ($(ARCH),sparc)
VPX_ARCH := sparc
else ifeq ($(ARCH),x86_64)
VPX_ARCH := x86_64
endif

ifdef HAVE_ANDROID
VPX_OS := android
else ifdef HAVE_LINUX
VPX_OS := linux
else ifdef HAVE_DARWIN_OS
ifeq ($(IOS_TARGET_PLATFORM),iPhoneSimulator)
    ifeq ($(ARCH),arm64)
        VPX_OS := iphonesimulator
    else ifeq ($(ARCH),x86_64)
        VPX_OS := iphonesimulator
    endif
else ifdef HAVE_IOS
VPX_OS := darwin
else
# To build for arm64 on macOS, we need Darwin version 20 or higher
VPX_OS := darwin20
endif
else ifdef HAVE_SOLARIS
VPX_OS := solaris
else ifdef HAVE_WIN64 # must be before WIN32
VPX_OS := win64
else ifdef HAVE_WIN32
VPX_OS := win32
else ifdef HAVE_BSD
VPX_OS := linux
endif

VPX_TARGET := generic-gnu
ifdef VPX_ARCH
ifdef VPX_OS
ifeq ($(IOS_TARGET_PLATFORM),iPhoneSimulator)
    ifeq ($(ARCH),arm64)
        VPX_TARGET := arm64-iphonesimulator-gcc
    else ifeq ($(ARCH),x86_64)
        VPX_TARGET := x86_64-iphonesimulator-gcc
    endif
else
    VPX_TARGET := $(VPX_ARCH)-$(VPX_OS)-gcc
endif
endif
endif

VPX_CONF := \
	--as=yasm \
	--disable-docs \
	--disable-tools \
	--disable-examples \
	--disable-unit-tests \
	--disable-install-bins \
	--disable-install-docs \
	--enable-realtime-only \
	--enable-error-concealment \
	--disable-webm-io

ifdef HAVE_IOS
    VPX_CONF += \
        --disable-runtime-cpu-detect \
        --disable-neon # Disable NEON to prevent crashes on iOS devices A11 and prior
endif

ifndef HAVE_WIN32
VPX_CONF += --enable-pic
endif
LOCAL_HOSTVARS=
ifdef HAVE_ANDROID
VPX_CONF += --disable-tools --extra-cflags="-I$(ANDROID_NDK)/sources/android/cpufeatures/ -fvisibility=hidden"
LOCAL_HOSTVARS=$(HOSTVARS) LD=$(CC)
endif
ifdef HAVE_WIN32
ifeq ($(ARCH),i386)
VPX_CONF += --extra-cflags="-mstackrealign"
endif
endif
.vpx: libvpx
	cd $< && CROSS=$(VPX_CROSS) $(LOCAL_HOSTVARS) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" ./configure --force-target=$(VPX_TARGET) \
		$(VPX_CONF) --prefix=$(PREFIX)
	cd $< && $(MAKE)
	cd $< && ../../../contrib/src/pkg-static.sh vpx.pc
	cd $< && $(MAKE) install
	touch $@
