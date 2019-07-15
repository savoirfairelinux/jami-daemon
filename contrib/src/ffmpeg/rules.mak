FFMPEG_HASH := 5ab44ff20cdc0e05adecbd0cd352d25fcb930094
FFMPEG_URL := https://git.ffmpeg.org/gitweb/ffmpeg.git/snapshot/$(FFMPEG_HASH).tar.gz

PKGS+=ffmpeg

ifeq ($(call need_pkg,"libavutil >= 55.75.100 libavcodec >= 57.106.101 libavformat >= 57.82.100 libavdevice >= 57.8.101 libavfilter >= 6.105.100 libswscale >= 4.7.103 libswresample >= 2.9.100"),)
PKGS_FOUND += ffmpeg
endif

DEPS_ffmpeg = iconv zlib vpx opus speex x264

FFMPEGCONF = \
	--cc="$(CC)" \
	--pkg-config="$(PKG_CONFIG)"

#disable everything
FFMPEGCONF += \
	--disable-everything \
	--enable-zlib \
	--enable-gpl \
	--enable-swscale \
	--enable-bsfs \
	--disable-filters \
	--disable-programs \
	--disable-postproc

FFMPEGCONF += \
	--disable-protocols \
	--enable-protocol=crypto \
	--enable-protocol=file \
	--enable-protocol=rtp \
	--enable-protocol=srtp \
	--enable-protocol=tcp \
	--enable-protocol=udp \
	--enable-protocol=unix \
	--enable-protocol=pipe

#enable muxers/demuxers
FFMPEGCONF += \
	--disable-demuxers \
	--disable-muxers \
	--enable-muxer=rtp \
	--enable-muxer=g722 \
	--enable-muxer=h263 \
	--enable-muxer=h264 \
	--enable-muxer=hevc \
	--enable-muxer=webm \
	--enable-muxer=ogg \
	--enable-muxer=pcm_s16be \
	--enable-muxer=pcm_s16le \
	--enable-demuxer=rtp \
	--enable-demuxer=mjpeg \
	--enable-demuxer=mjpeg_2000 \
	--enable-demuxer=mpegvideo \
	--enable-demuxer=gif \
	--enable-demuxer=image_jpeg_pipe \
	--enable-demuxer=image_png_pipe \
	--enable-demuxer=image_webp_pipe \
	--enable-demuxer=matroska \
	--enable-demuxer=m4v \
	--enable-demuxer=mp3 \
	--enable-demuxer=ogg \
	--enable-demuxer=flac \
	--enable-demuxer=wav \
	--enable-demuxer=ac3 \
	--enable-demuxer=g722 \
	--enable-demuxer=pcm_mulaw \
	--enable-demuxer=pcm_alaw \
	--enable-demuxer=pcm_s16be \
	--enable-demuxer=pcm_s16le \
	--enable-demuxer=h263 \
	--enable-demuxer=h264 \
	--enable-demuxer=hevc

#enable parsers
FFMPEGCONF += \
	--enable-parser=h263 \
	--enable-parser=h264 \
	--enable-parser=mpeg4video \
	--enable-parser=vp8 \
	--enable-parser=vp9 \
	--enable-parser=opus

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
	--enable-decoder=vp9 \
	--enable-encoder=h263 \
	--enable-encoder=h263p \
	--enable-decoder=h263 \
	--enable-encoder=mjpeg \
	--enable-decoder=mjpeg \
	--enable-decoder=mjpegb \
	--enable-libspeex \
	--enable-libopus \
	--enable-libvpx \
	--enable-libx264 \
	--enable-encoder=libspeex \
	--enable-decoder=libspeex \
	--enable-encoder=libopus \
	--enable-decoder=libopus

# decoders for ringtones and audio streaming
FFMPEGCONF += \
	--enable-decoder=flac \
	--enable-decoder=vorbis \
	--enable-decoder=aac \
	--enable-decoder=ac3 \
	--enable-decoder=eac3 \
	--enable-decoder=mp3 \
	--enable-decoder=pcm_u24be \
	--enable-decoder=pcm_u24le \
	--enable-decoder=pcm_u32be \
	--enable-decoder=pcm_u32le \
	--enable-decoder=pcm_u8 \
	--enable-decoder=pcm_f16le \
	--enable-decoder=pcm_f24le \
	--enable-decoder=pcm_f32be \
	--enable-decoder=pcm_f32le \
	--enable-decoder=pcm_f64be \
	--enable-decoder=pcm_f64le \
	--enable-decoder=pcm_s16be \
	--enable-decoder=pcm_s16be_planar \
	--enable-decoder=pcm_s16le \
	--enable-decoder=pcm_s16le_planar \
	--enable-decoder=pcm_s24be \
	--enable-decoder=pcm_s24le \
	--enable-decoder=pcm_s24le_planar \
	--enable-decoder=pcm_s32be \
	--enable-decoder=pcm_s32le \
	--enable-decoder=pcm_s32le_planar \
	--enable-decoder=pcm_s64be \
	--enable-decoder=pcm_s64le \
	--enable-decoder=pcm_s8 \
	--enable-decoder=pcm_s8_planar \
	--enable-decoder=pcm_u16be \
	--enable-decoder=pcm_u16le

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

#filters
FFMPEGCONF += \
	--enable-filter=scale \
	--enable-filter=overlay \
	--enable-filter=amix \
	--enable-filter=amerge \
	--enable-filter=aresample \
	--enable-filter=format \
	--enable-filter=aformat \
	--enable-filter=fps \
	--enable-filter=transpose \
	--enable-filter=pad

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
DEPS_ffmpeg += ffnvcodec media-sdk
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
	--enable-hwaccel=h263_vaapi \
	--enable-hwaccel=vp8_vaapi \
	--enable-hwaccel=mjpeg_vaapi \
	--enable-encoder=h264_vaapi \
	--enable-encoder=vp8_vaapi \
	--enable-encoder=mjpeg_vaapi \
	--enable-cuvid \
	--enable-ffnvcodec \
	--enable-nvdec \
	--enable-nvenc \
	--enable-hwaccel=h264_nvdec \
	--enable-hwaccel=hevc_nvdec \
	--enable-hwaccel=vp8_nvdec \
	--enable-hwaccel=mjpeg_nvdec \
	--enable-encoder=h264_nvenc \
	--enable-encoder=hevc_nvenc \
 	--enable-libmfx \
 	--enable-encoder=h264_qsv \
 	--enable-encoder=hevc_qsv \
    --enable-encoder=mjpeg_qsv \
 	--enable-decoder=vp8_qsv \
 	--enable-decoder=h264_qsv \
 	--enable-decoder=hevc_qsv \
    --enable-filter=scale_qsv \
    --enable-filter=overlay_qsv

endif
endif

ifdef HAVE_MACOSX
FFMPEGCONF += \
	--enable-avfoundation \
	--enable-indev=avfoundation \
	--enable-videotoolbox \
	--enable-hwaccel=h263_videotoolbox \
	--enable-hwaccel=h264_videotoolbox \
	--enable-hwaccel=mpeg4_videotoolbox \
	--enable-encoder=h264_videotoolbox \
	--disable-securetransport
endif

ifdef HAVE_IOS
FFMPEGCONF += \
	--enable-videotoolbox \
	--enable-hwaccel=h263_videotoolbox \
	--enable-hwaccel=h264_videotoolbox \
	--enable-hwaccel=mpeg4_videotoolbox \
	--enable-encoder=h264_videotoolbox \
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
	$(APPLY) $(SRC)/ffmpeg/remove-mjpeg-log.patch
	$(APPLY) $(SRC)/ffmpeg/change-RTCP-ratio.patch
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/ffmpeg/avfoundation_fix.patch
endif
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.ffmpeg: ffmpeg .sum-ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-cflags="$(CFLAGS)" \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
