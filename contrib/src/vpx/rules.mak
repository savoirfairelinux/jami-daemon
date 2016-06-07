# libvpx

#Libav doesnt support new vpx version
ifdef HAVE_IOS
VPX_HASH := cbecf57f3e0d85a7b7f97f3ab7c507f6fe640a93
else
VPX_HASH := c74bf6d889992c3cabe017ec353ca85c323107cd
endif
VPX_URL := https://github.com/webmproject/libvpx/archive/$(VPX_HASH).tar.gz
#VPX_GITURL := https://code.google.com/p/webm.libvpx

$(TARBALLS)/libvpx-$(VPX_HASH).tar.gz:
	$(call download,$(VPX_URL))

.sum-vpx: libvpx-$(VPX_HASH).tar.gz
	$(warning $@ not implemented)
	touch $@

libvpx: libvpx-$(VPX_HASH).tar.gz .sum-vpx
	rm -Rf $@-$(VPX_HASH)
	mkdir -p $@-$(VPX_HASH)
	(cd $@-$(VPX_HASH) && tar xv --strip-components=1 -f ../$<)
	$(MOVE)

DEPS_vpx =

ifdef HAVE_CROSS_COMPILE
ifndef HAVE_IOS
VPX_CROSS := $(CROSS_COMPILE)
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
VPX_OS := iphonesimulator
else ifeq ($(ARCH),armv7)
VPX_OS := darwin
else ifeq ($(ARCH),arm64)
VPX_OS := darwin
else
ifeq ($(OSX_VERSION),10.5)
VPX_OS := darwin9
else
VPX_OS := darwin10
endif
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
VPX_TARGET := $(VPX_ARCH)-$(VPX_OS)-gcc
endif
endif

VPX_CONF := \
	--as=yasm \
	--disable-docs \
	--disable-examples \
	--disable-unit-tests \
	--disable-install-bins \
	--disable-install-docs \
	--enable-realtime-only \
	--enable-error-concealment \
	--disable-runtime-cpu-detect \
	--disable-webm-io

ifdef HAVE_WIN32
VPX_CONF +=	--disable-ssse3
endif

ifndef HAVE_WIN32
VPX_CONF += --enable-pic
endif
ifdef HAVE_MACOSX
VPX_CONF += --sdk-path=$(MACOSX_SDK)
endif
ifdef HAVE_IOS
VPX_CONF += --sdk-path=$(IOS_SDK)
endif
LOCAL_HOSTVARS=
ifdef HAVE_ANDROID
# vpx configure.sh overrides our sysroot and it looks for it itself, and
# uses that path to look for the compiler (which we already know)
VPX_CONF += --sdk-path=$(shell dirname $(shell which $(CROSS_COMPILE)gcc))
# needed for cpu-features.h
VPX_CONF += --extra-cflags="-I $(ANDROID_NDK)/sources/cpufeatures/"
# set an explicit alternative libc since the sysroot override can make it blank
VPX_CONF += --libc=$(SYSROOT)
LOCAL_HOSTVARS=$(HOSTVARS)
endif

.vpx: libvpx
	cd $< && CROSS=$(VPX_CROSS) $(LOCAL_HOSTVARS) ./configure --target=$(VPX_TARGET) \
		$(VPX_CONF) --prefix=$(PREFIX)
	cd $< && $(MAKE)
	cd $< && ../../../contrib/src/pkg-static.sh vpx.pc
	cd $< && $(MAKE) install
	touch $@
