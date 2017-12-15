FFMPEG_HASH := 18516d3e695980525bd9758dc7b8a8e36cd3f09e
FFMPEG_URL := https://git.ffmpeg.org/gitweb/ffmpeg.git/snapshot/$(FFMPEG_HASH).tar.gz

PKGS+=ffmpeg

ifeq ($(call need_pkg,"libavutil >= 55.75.100 libavcodec >= 57.106.101 libavformat >= 57.82.100 libavdevice >= 57.8.101 libswscale >= 4.7.103"),)
PKGS_FOUND += ffmpeg
endif

DEPS_ffmpeg = iconv zlib x264 vpx opus speex

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
	--enable-bsfs \
	--disable-programs

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
	--enable-jni \
	--enable-mediacodec \
	--enable-hwaccel=vp8_mediacodec \
	--enable-hwaccel=h264_mediacodec \
	--enable-hwaccel=mpeg4_mediacodec \
	--enable-decoder=vp8_mediacodec \
	--enable-decoder=h264_mediacodec \
	--enable-decoder=mpeg4_mediacodec
# ASM not working on Android x86 https://trac.ffmpeg.org/ticket/4928
ifeq ($(ARCH),i386)
FFMPEGCONF += --disable-asm
endif
ifeq ($(ARCH),x86_64)
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
	--enable-avfoundation \
	--enable-indev=avfoundation \
	--enable-videotoolbox \
	--enable-hwaccel=h263_videotoolbox \
	--enable-hwaccel=h264_videotoolbox \
	--enable-hwaccel=mpeg4_videotoolbox
endif

ifdef HAVE_IOS
FFMPEGCONF += \
	--target-os=darwin \
	--enable-cross-compile \
	--arch=$(ARCH) \
	--enable-pic
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
FFMPEGCONF += --arch=arm
ifdef HAVE_ARMV7A
FFMPEGCONF += --cpu=cortex-a8
endif
ifdef HAVE_ARMV6
FFMPEGCONF += --cpu=armv6 --disable-neon
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

ffmpeg: ffmpeg-$(FFMPEG_HASH).tar.gz
	rm -Rf $@ $@-$(FFMPEG_HASH)
	mkdir -p $@-$(FFMPEG_HASH)
	(cd $@-$(FFMPEG_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f ../$<)
	$(UPDATE_AUTOCONFIG)
ifdef HAVE_ANDROID
ifeq ($(ARCH),arm)
	$(APPLY) $(SRC)/ffmpeg/android_file_offset.patch
endif
ifeq ($(ARCH),i386)
	$(APPLY) $(SRC)/ffmpeg/android_file_offset.patch
endif
endif
	$(MOVE)

.ffmpeg: ffmpeg .sum-ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-cflags="$(CFLAGS)" \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
