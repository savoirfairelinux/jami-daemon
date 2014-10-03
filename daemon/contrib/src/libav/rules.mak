HASH=HEAD
LIBAV_SNAPURL := http://git.libav.org/?p=libav.git;a=snapshot;h=$(HASH);sf=tgz

LIBAVCONF = \
        --cc="$(CC)"                 \
        --pkg-config="$(PKG_CONFIG)" \
        --disable-everything         \
        --enable-gpl                 \
        --enable-version3            \
        --enable-decoders            \
        --enable-protocols           \
        --enable-demuxers            \
        --enable-muxers              \
        --enable-swscale

#encoders
LIBAVCONF += \
        --enable-libx264             \
        --enable-encoder=libvpx      \
        --disable-decoder=libvpx     \
        --disable-decoder=libvpx_vp8 \
        --disable-decoder=libvpx_vp9 \
        --enable-encoder=h263p


DEPS_libav = zlib x264 vpx $(DEPS_vpx)


ifdef HAVE_CROSS_COMPILE
LIBAVCONF += --enable-cross-compile
ifndef HAVE_DARWIN_OS
LIBAVCONF += --cross-prefix=$(HOST)-
endif
endif

# ARM stuff
ifeq ($(ARCH),arm)
ifdef HAVE_ARMV7A
endif
ifndef HAVE_DARWIN_OS
LIBAVCONF += --arch=arm
endif
ifdef HAVE_NEON
LIBAVCONF += --enable-neon
endif
ifdef HAVE_ARMV7A
LIBAVCONF += --cpu=cortex-a8
LIBAVCONF += --enable-thumb
endif
ifdef HAVE_ARMV6
LIBAVCONF += --cpu=armv6 --disable-neon
endif
endif

# x86 stuff
ifeq ($(ARCH),i386)
ifndef HAVE_DARWIN_OS
LIBAVCONF += --arch=x86
endif
endif

# Darwin
ifdef HAVE_DARWIN_OS
LIBAVCONF += --arch=$(ARCH) --target-os=darwin
ifeq ($(ARCH),x86_64)
LIBAVCONF += --cpu=core2
endif
endif
ifdef HAVE_IOS
LIBAVCONF += --enable-pic
ifdef HAVE_NEON
LIBAVCONF += --as="$(AS)"
endif
endif
#ifdef HAVE_MACOSX
#LIBAVCONF += --enable-vda
#endif

# Linux
ifdef HAVE_LINUX
LIBAVCONF += --target-os=linux --enable-pic
ifndef HAVE_ANDROID
LIBAVCONF += --enable-indev=v4l2 --enable-indev=x11grab --enable-x11grab
endif
endif

# Windows
ifdef HAVE_WIN32
#ifndef HAVE_MINGW_W64
#DEPS_libav += directx
#endif

LIBAVCONF += --target-os=mingw32 --enable-memalign-hack
LIBAVCONF += --enable-w32threads --disable-decoder=dca
#LIBAVCONF += --enable-dxva2

ifdef HAVE_WIN64
LIBAVCONF += --cpu=athlon64 --arch=x86_64
else # !WIN64
LIBAVCONF+= --cpu=i686 --arch=x86
endif

else # !Windows
LIBAVCONF += --enable-pthreads
endif

# Only build on Linux for now, since no one else has implemented video
ifdef HAVE_LINUX
PKGS += libav
endif
ifeq ($(call need_pkg,"libavcodec >= 53.5.0 libavformat >= 53.2.0 libswscale libavdevice >= 53.0.0 libavutil >= 51.0.0"),)
PKGS_FOUND += libav
endif

$(TARBALLS)/libav-$(HASH).tar.gz:
	$(call download,$(LIBAV_SNAPURL))

.sum-libav: $(TARBALLS)/libav-$(HASH).tar.gz
	$(warning Not implemented.)
	touch $@

libav: libav-$(HASH).tar.gz .sum-libav
	rm -Rf $@ $@-$(HASH)
	mkdir -p $@-$(HASH)
	$(ZCAT) "$<" | (cd $@-$(HASH) && tar xv --strip-components=1)
	$(MOVE)

.libav: libav
	cd $< && $(HOSTVARS) ./configure \
		--extra-ldflags="$(LDFLAGS)" $(LIBAVCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
