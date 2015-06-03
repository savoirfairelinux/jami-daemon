FFMPEG_HASH := f90c9bbbca32620a9cb78898a2b1b7d2cce9ad0f
FFMPEG_GITURL := https://github.com/FFmpeg/FFmpeg.git

ifdef HAVE_WIN32
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
FFMPEGCONF += \
		--enable-libx264 \
		--enable-libvpx

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
		--enable-decoder=h263 \
		--enable-encoder=mjpeg \
		--enable-decoder=mjpeg \
		--enable-decoder=mjpegb \
		--enable-libspeex \
		--enable-libopus \
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

FFMPEGCONF += \
	--enable-indev=dshow \
	--enable-indev=gdigrab \
	--enable-dxva2

DEPS_ffmpeg = iconv zlib x264 vpx opus speex $(DEPS_vpx)

ifdef HAVE_CROSS_COMPILE
FFMPEGCONF += --cross-prefix=$(HOST)-
endif

# x86 stuff
ifeq ($(ARCH),i386)
FFMPEGCONF += --arch=x86
endif

ifeq ($(ARCH),x86_64)
FFMPEGCONF += --arch=x86_64
endif

# Windows
ifdef HAVE_WIN32
FFMPEGCONF += --target-os=mingw32 --enable-memalign-hack
FFMPEGCONF += --enable-w32threads --disable-decoder=dca
endif

ifeq ($(call need_pkg,"ffmpeg >= 2.6.1"),)
PKGS_FOUND += ffmepg
endif

$(TARBALLS)/ffmpeg-$(FFMPEG_HASH).tar.xz:
	$(call download_git,$(FFMPEG_GITURL),release/2.6, $(FFMPEG_HASH))

.sum-ffmpeg: ffmpeg-$(FFMPEG_HASH).tar.xz
	$(warning Not implemented.)
	touch $@

ffmpeg: ffmpeg-$(FFMPEG_HASH).tar.xz .sum-ffmpeg
	rm -Rf $@ $@-$(FFMPEG_HASH)
	mkdir -p $@-$(FFMPEG_HASH)
	(cd $@-$(FFMPEG_HASH) && tar xv --strip-components=1 -f ../$<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.ffmpeg: ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
