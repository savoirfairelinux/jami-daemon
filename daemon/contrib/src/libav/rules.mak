#Libav 11-1 (git version packaged for ubuntu  14.10)
LIBAV_HASH := f851477889ae48e2f17073cf7486e1d5561b7ae4
LIBAV_GITURL := git://git.libav.org/libav.git

PKGS += libav

LIBAVCONF = \
		--cc="$(CC)" \
		--pkg-config="$(PKG_CONFIG)" \
		--enable-zlib \
		--enable-gpl \
		--enable-swscale \
		--enable-protocols

#disable everything
LIBAVCONF += \
		--disable-everything

#enable muxers/demuxers
LIBAVCONF += \
		--enable-demuxers \
		--enable-muxers

#enable parsers
LIBAVCONF += \
		--enable-parser=h263 \
		--enable-parser=h264 \
		--enable-parser=mpeg4video \
		--enable-parser=opus \
		--enable-parser=vp8

#librairies
LIBAVCONF += \
		--enable-libx264 \
		--enable-libvpx

#encoders/decoders
LIBAVCONF += \
		--enable-encoder=adpcm_g722 \
		--enable-decoder=adpcm_g722 \
		--enable-encoder=rawvideo \
		--enable-decoder=rawvideo \
		--enable-encoder=libx264 \
		--enable-decoder=h264 \
		--enable-encoder=pcm_alaw \
		--enable-decoder=pcm_alaw \
		--enable-encoder=pcm_mulaw \
		--enable-decoder=pcm_mulaw \
		--enable-encoder=libopus \
		--enable-decoder=libopus \
		--enable-encoder=mpeg4 \
		--enable-decoder=mpeg4 \
		--enable-encoder=libvpx_vp8 \
		--enable-decoder=vp8 \
		--enable-encoder=h263 \
		--enable-decoder=h263

# Linux
ifdef HAVE_LINUX
LIBAVCONF += \
	--enable-x11grab
endif

# There is an unresolved symbol for speex when linking statically
ifndef HAVE_DARWIN_OS
ifndef HAVE_WIN32
LIBAVCONF += \
          --enable-libspeex \
          --enable-libopus \
          --enable-encoder=libspeex \
          --enable-decoder=libspeex
endif
endif

DEPS_libav = zlib x264 vpx opus speex $(DEPS_vpx)

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

ifeq ($(call need_pkg,"libavcodec >= 53.5.0 libavformat >= 54.20.3 libswscale libavdevice >= 53.0.0 libavutil >= 51.0.0"),)
PKGS_FOUND += libav
endif

$(TARBALLS)/libav-$(LIBAV_HASH).tar.xz:
	$(call download_git,$(LIBAV_GITURL),master,$(LIBAV_HASH))

.sum-libav: libav-$(LIBAV_HASH).tar.xz
	$(warning Not implemented.)
	touch $@

libav: libav-$(LIBAV_HASH).tar.xz .sum-libav
	rm -Rf $@ $@-$(LIBAV_HASH)
	mkdir -p $@-$(LIBAV_HASH)
	(cd $@-$(LIBAV_HASH) && tar xv --strip-components=1 -f ../$<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.libav: libav
	cd $< && $(HOSTVARS) ./configure \
		--extra-ldflags="$(LDFLAGS)" $(LIBAVCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
