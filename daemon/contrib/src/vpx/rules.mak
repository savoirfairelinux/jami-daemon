# libvpx

VPX_HASH := 4640a0c4804b49f1870d5a2d17df0c7d0a77af2f
VPX_URL := http://libvpx.webm.googlecode.com/archive/$(VPX_HASH).tar.gz
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
VPX_CROSS := $(HOST)-
else
VPX_CROSS :=
endif

ifeq ($(ARCH),arm)
VPX_ARCH := armv7
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
ifeq ($(ARCH),arm)
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

ifndef HAVE_WIN32
VPX_CONF += --enable-pic
endif
ifdef HAVE_MACOSX
VPX_CONF += --sdk-path=$(MACOSX_SDK)
endif
ifdef HAVE_IOS
VPX_CONF += --sdk-path=$(SDKROOT)
endif
ifdef HAVE_ANDROID
# vpx configure.sh overrides our sysroot and it looks for it itself, and
# uses that path to look for the compiler (which we already know)
VPX_CONF += --sdk-path=$(shell dirname $(shell which $(HOST)-gcc))
# needed for cpu-features.h
VPX_CONF += --extra-cflags="-I $(ANDROID_NDK)/sources/cpufeatures/"
endif

.vpx: libvpx
	cd $< && CROSS=$(VPX_CROSS) ./configure --target=$(VPX_TARGET) \
		$(VPX_CONF) --prefix=$(PREFIX)
	cd $< && $(MAKE)
	cd $< && ../../../contrib/src/pkg-static.sh vpx.pc
	cd $< && $(MAKE) install
	touch $@
