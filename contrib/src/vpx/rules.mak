# libvpx

VPX_HASH := v1.8.1
VPX_URL := https://github.com/webmproject/libvpx/archive/$(VPX_HASH).tar.gz

$(TARBALLS)/libvpx-$(VPX_HASH).tar.gz:
	$(call download,$(VPX_URL))

.sum-vpx: libvpx-$(VPX_HASH).tar.gz

libvpx: libvpx-$(VPX_HASH).tar.gz .sum-vpx
	rm -Rf $@-$(VPX_HASH)
	mkdir -p $@-$(VPX_HASH)
	(cd $@-$(VPX_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f $<)
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
	--disable-tools \
	--disable-examples \
	--disable-unit-tests \
	--disable-install-bins \
	--disable-install-docs \
	--enable-realtime-only \
	--enable-error-concealment \
	--disable-webm-io

ifdef HAVE_IOS
	VPX_CONF += --disable-runtime-cpu-detect
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
	cd $< && CROSS=$(VPX_CROSS) $(LOCAL_HOSTVARS) ./configure --target=$(VPX_TARGET) \
		$(VPX_CONF) --prefix=$(PREFIX)
	cd $< && $(MAKE)
	cd $< && ../../../contrib/src/pkg-static.sh vpx.pc
	cd $< && $(MAKE) install
	touch $@
