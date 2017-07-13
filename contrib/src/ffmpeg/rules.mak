FFMPEG_HASH := n3.3.1
FFMPEG_URL := https://git.ffmpeg.org/gitweb/ffmpeg.git/snapshot/$(FFMPEG_HASH).tar.gz

PKGS+=ffmpeg

ifeq ($(call need_pkg,"libavcodec >= 57.89.100 libavformat >= 57.71.100 libswscale >= 4.6.100 libavdevice >= 57.6.100 libavutil >= 55.58.100"),)
PKGS_FOUND += ffmpeg
endif

DEPS_ffmpeg = iconv zlib x264 vpx opus speex $(DEPS_vpx)

FFMPEGCONF = \
	--cc="$(CC)" \
	--pkg-config="$(PKG_CONFIG)"

#disable everything
FFMPEGCONF += \
	--disable-everything \
	--enable-zlib \
	--enable-gpl \
	--enable-swscale \
	--enable-protocols \
	--disable-programs \
	--disable-sdl

#enable muxers/demuxers
FFMPEGCONF += \
	--enable-demuxers \
	--enable-muxers

#enable parsers
FFMPEGCONF += \
	--enable-parser=h263 \
	--enable-parser=h264 \
	--enable-parser=mpeg4video \
	--enable-parser=vp8

#encoders/decoders
FFMPEGCONF += \
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
	--enable-encoder=mpeg4 \
	--enable-decoder=mpeg4 \
	--enable-encoder=libvpx_vp8 \
	--enable-decoder=vp8 \
	--enable-encoder=h263 \
	--enable-encoder=h263p \
	--enable-decoder=h263 \
	--enable-encoder=mjpeg \
	--enable-decoder=mjpeg \
	--enable-decoder=mjpegb \
	--enable-libspeex \
	--enable-libopus \
	--enable-libvpx \
	--enable-encoder=libspeex \
	--enable-decoder=libspeex \
	--enable-encoder=libopus \
	--enable-decoder=libopus

#encoders/decoders for images
FFMPEGCONF += \
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

#platform specific options

ifdef HAVE_WIN32
FFMPEGCONF += \
	--enable-indev=dshow \
	--enable-indev=gdigrab \
	--enable-dxva2
endif

ifdef HAVE_LINUX
FFMPEGCONF += --enable-pic
FFMPEGCONF += --extra-cxxflags=-fPIC --extra-cflags=-fPIC
ifdef HAVE_ANDROID
# Android Linux
FFMPEGCONF += \
	--target-os=android \
	--enable-jni
# ARM 32 bits has trouble with mediacodec
ifneq ($(ARCH),arm)
FFMPEGCONF += \
	--enable-mediacodec \
	--enable-hwaccel=vp8_mediacodec \
	--enable-hwaccel=mpeg4_mediacodec \
	--enable-decoder=vp8_mediacodec \
	--enable-decoder=mpeg4_mediacodec
endif
# ASM not working on Android x86 https://trac.ffmpeg.org/ticket/4928
ifeq ($(ARCH),i386)
FFMPEGCONF += --disable-asm
endif
else
# Desktop Linux
FFMPEGCONF += \
	--target-os=linux \
	--enable-indev=v4l2 \
	--enable-indev=xcbgrab \
	--enable-vdpau \
	--enable-hwaccel=h264_vdpau \
	--enable-hwaccel=mpeg4_vdpau \
	--enable-vaapi \
	--enable-hwaccel=h264_vaapi \
	--enable-hwaccel=mpeg4_vaapi \
	--enable-hwaccel=h263_vaapi
endif
endif

ifndef HAVE_ANDROID
FFMPEGCONF += --enable-libx264
endif

ifdef HAVE_MACOSX
FFMPEGCONF += \
	--enable-indev=avfcapture \
	--enable-indev=avfgrab \
	--enable-videotoolbox \
	--enable-hwaccel=h263_videotoolbox \
	--enable-hwaccel=h264_videotoolbox \
	--enable-hwaccel=mpeg4_videotoolbox \
	--enable-vda \
	--enable-hwaccel=h264_vda
endif

ifdef HAVE_IOS
FFMPEGCONF += \
	--target-os=darwin \
	--enable-cross-compile \
	--arch=$(ARCH) \
	--enable-pic \
	--enable-indev=avfoundation
endif

ifndef HAVE_IOS
ifdef HAVE_CROSS_COMPILE
FFMPEGCONF += --cross-prefix=$(HOST)-
endif
endif

# x86 stuff
ifeq ($(ARCH),i386)
FFMPEGCONF += --arch=x86
endif

ifeq ($(ARCH),x86_64)
FFMPEGCONF += --arch=x86_64
endif

# ARM stuff
ifeq ($(ARCH),arm)
# neon causes SIGBUS error on ARM 32 bits
FFMPEGCONF += --disable-neon
FFMPEGCONF += --arch=arm
ifdef HAVE_ARMV7A
FFMPEGCONF += --cpu=cortex-a8
endif
ifdef HAVE_ARMV6
FFMPEGCONF += --cpu=armv6
endif
endif

# ARM64 stuff
ifeq ($(ARCH),aarch64)
FFMPEGCONF += --arch=aarch64
endif
ifeq ($(ARCH),arm64)
FFMPEGCONF += --arch=aarch64
endif

# Windows
ifdef HAVE_WIN32
FFMPEGCONF += --target-os=mingw32
FFMPEGCONF += --enable-w32threads --disable-decoder=dca
endif

$(TARBALLS)/ffmpeg-$(FFMPEG_HASH).tar.gz:
	$(call download,$(FFMPEG_URL))

.sum-ffmpeg: ffmpeg-$(FFMPEG_HASH).tar.gz
	$(warning $@ is not implemented.)
	touch $@

ffmpeg: ffmpeg-$(FFMPEG_HASH).tar.gz .sum-ffmpeg
	rm -Rf $@ $@-$(FFMPEG_HASH)
	mkdir -p $@-$(FFMPEG_HASH)
	(cd $@-$(FFMPEG_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f ../$<)
	$(UPDATE_AUTOCONFIG)
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/ffmpeg/0004-add-avfcapture-device.patch
	$(APPLY) $(SRC)/ffmpeg/0005-add-avfgrab-device.patch
endif
	$(MOVE)

.ffmpeg: ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-cflags="$(CFLAGS)" \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
