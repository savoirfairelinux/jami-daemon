FFMPEG_HASH := c46d22a4a58467bdc7885685b06a2114dd181c43
FFMPEG_URL := https://git.ffmpeg.org/gitweb/ffmpeg.git/snapshot/$(FFMPEG_HASH).tar.gz

ifdef HAVE_WIN32
PKGS += ffmpeg
endif

ifdef HAVE_LINUX
PKGS += ffmpeg
endif

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

#librairies
ifndef HAVE_ANDROID
FFMPEGCONF += --enable-libx264
endif

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

ifdef HAVE_WIN32
FFMPEGCONF += \
	--enable-indev=dshow \
	--enable-indev=gdigrab \
	--enable-dxva2
endif

ifdef HAVE_LINUX
FFMPEGCONF += \
	--disable-vdpau \
	--enable-vaapi \
	--enable-hwaccel=h264_vaapi \
	--enable-hwaccel=mpeg4_vaapi \
	--enable-hwaccel=h263_vaapi
endif

ifdef HAVE_MACOSX
FFMPEGCONF += \
	--enable-indev=avfcapture \
	--enable-indev=avfgrab \
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
	--enable-pic \
        --enable-indev=avfoundation
endif

DEPS_ffmpeg = iconv zlib x264 vpx opus speex $(DEPS_vpx)

# Linux
ifdef HAVE_LINUX
FFMPEGCONF += --target-os=linux --enable-pic
ifndef HAVE_ANDROID
FFMPEGCONF += --enable-indev=v4l2 --enable-indev=x11grab_xcb --enable-indev=x11grab --enable-x11grab
else
# used to avoid Text Relocations
FFMPEGCONF += --extra-cxxflags=-fPIC --extra-cflags=-fPIC
FFMPEGCONF += --disable-asm
endif
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
ifdef HAVE_NEON
FFMPEGCONF += --enable-neon
endif
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
FFMPEGCONF += --target-os=mingw32 --enable-memalign-hack
FFMPEGCONF += --enable-w32threads --disable-decoder=dca
endif

ifeq ($(call need_pkg,"libavcodec >= 57.48.101 libavformat >= 57.41.100 libswscale >= 4.1.100 libavdevice >= 57.0.101 libavutil >= 55.28.100"),)
PKGS_FOUND += ffmpeg
endif

$(TARBALLS)/ffmpeg-$(FFMPEG_HASH).tar.xz:
	$(call download,$(FFMPEG_URL))

.sum-ffmpeg: ffmpeg-$(FFMPEG_HASH).tar.xz
	$(warning $@ is not implemented.)
	touch $@

ffmpeg: ffmpeg-$(FFMPEG_HASH).tar.xz .sum-ffmpeg
	rm -Rf $@ $@-$(FFMPEG_HASH)
	mkdir -p $@-$(FFMPEG_HASH)
	(cd $@-$(FFMPEG_HASH) && tar x $(if ${BATCH_MODE},,-v) --strip-components=1 -f ../$<)
	$(UPDATE_AUTOCONFIG)
	$(APPLY) $(SRC)/ffmpeg/0004-avformat-fix-find_stream_info-not-considering-extradata.patch
ifdef HAVE_IOS
	$(APPLY) $(SRC)/ffmpeg/clock_gettime.patch
endif
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/ffmpeg/clock_gettime.patch
endif
	$(MOVE)

.ffmpeg: ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-cflags="$(CFLAGS)" \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
                --prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
