#Libav 11-1 (git version packaged for ubuntu  14.10)
LIBAV_HASH := f851477889ae48e2f17073cf7486e1d5561b7ae4
LIBAV_URL := https://git.libav.org/?p=libav.git;a=snapshot;h=$(LIBAV_HASH);sf=tgz

ifndef HAVE_WIN32
PKGS += libav
endif

#disable everything
#ensure to add this option first
LIBAVCONF = \
		--disable-everything

LIBAVCONF += \
		--cc="$(CC)" \
		--pkg-config="$(PKG_CONFIG)" \
		--enable-zlib \
		--enable-gpl \
		--enable-swscale \
		--enable-protocols


#enable muxers/demuxers
LIBAVCONF += \
		--enable-demuxers \
		--enable-muxers

#enable parsers
LIBAVCONF += \
		--enable-parser=h263 \
		--enable-parser=mpeg4video \
		--enable-parser=opus

#librairies
LIBAVCONF += \
		--enable-libopus \
		--enable-libspeex

#encoders/decoders
LIBAVCONF += \
		--enable-encoder=adpcm_g722 \
		--enable-decoder=adpcm_g722 \
		--enable-encoder=rawvideo \
		--enable-decoder=rawvideo \
		--enable-encoder=pcm_alaw \
		--enable-decoder=pcm_alaw \
		--enable-encoder=pcm_mulaw \
		--enable-decoder=pcm_mulaw \
		--enable-encoder=libopus \
		--enable-decoder=libopus \
		--enable-encoder=mpeg4 \
		--enable-decoder=mpeg4 \
		--enable-encoder=h263 \
		--enable-decoder=h263 \
		--enable-encoder=h263p \
		--enable-encoder=libspeex \
		--enable-decoder=libspeex

#encoders/decoders for images
LIBAVCONF += \
		--enable-encoder=gif \
		--enable-decoder=gif \
		--enable-encoder=jpegls \
		--enable-decoder=jpegls \
		--enable-encoder=ljpeg \
		--enable-decoder=jpeg2000 \
		--enable-encoder=png \
		--enable-decoder=png \
		--enable-encoder=bmp \
		--enable-decoder=bmp \
		--enable-encoder=tiff \
		--enable-decoder=tiff
# Linux
ifndef HAVE_ANDROID
LIBAVCONF += \
		--enable-parser=h264 \
		--enable-libx264 \
		--enable-encoder=libx264 \
		--enable-decoder=h264 \
		--enable-parser=vp8 \
		--enable-libvpx \
		--enable-encoder=libvpx_vp8 \
		--enable-decoder=vp8
endif

DEPS_libav = zlib opus speex
ifndef HAVE_ANDROID
DEPS_libav += vpx x264 $(DEPS_vpx)
endif

ifdef HAVE_CROSS_COMPILE
LIBAVCONF += --enable-cross-compile
ifndef HAVE_DARWIN_OS
LIBAVCONF += --cross-prefix=$(CROSS_COMPILE)
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
LIBAVCONF += --arch=$(ARCH) --target-os=darwin --enable-indev=avfoundation
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

ifeq ($(call need_pkg,"libavcodec >= 53.5.0 libavformat >= 54.20.3 libswscale >= 1.1.0 libavdevice >= 53.0.0 libavutil >= 52.5.0"),)
PKGS_FOUND += libav
endif

$(TARBALLS)/libav-$(LIBAV_HASH).tar.xz:
	$(call download,$(LIBAV_URL))

.sum-libav: libav-$(LIBAV_HASH).tar.xz
	$(warning $@ is not implemented.)
	touch $@

libav: libav-$(LIBAV_HASH).tar.xz .sum-libav
	rm -Rf $@ $@-$(LIBAV_HASH)
	mkdir -p $@-$(LIBAV_HASH)
	(cd $@-$(LIBAV_HASH) && tar xv --strip-components=1 -f ../$<)
	$(UPDATE_AUTOCONFIG)
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/libav/0005-avfoundation-simple-capture.patch
	$(APPLY) $(SRC)/libav/0006-avfoundation-fix-framerate-selection.patch
endif
	$(APPLY) $(SRC)/libav/0001-rtpdec-add-a-trace-when-jitter-buffer-is-full.patch
	$(APPLY) $(SRC)/libav/0002-rtpdec-inform-jitter-buffer-size.patch
	$(APPLY) $(SRC)/libav/0003-rtsp-warning-when-max_delay-reached.patch
	$(APPLY) $(SRC)/libav/0004-mpegvideo_enc-enable-rtp_mode-when-multiple-slices-a.patch
	$(MOVE)

.libav: libav
	cd $< && $(HOSTVARS) ./configure \
		$(LIBAVCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
