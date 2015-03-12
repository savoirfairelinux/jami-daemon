FFMPEG_GITURL := https://github.com/FFmpeg/FFmpeg.git

PKGS += ffmpeg

FFMPEGCONF = \
		--cc="$(CC)" \
		--pkg-config="$(PKG_CONFIG)" \
		--enable-zlib \
		--enable-gpl \
		--enable-swscale \
		--enable-protocols

#disable everything
FFMPEGCONF += \
		--disable-everything \
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

FFMPEGCONF += \
	--enable-indev=dshow \
	--enable-dxva2

# There is an unresolved symbol for speex when linking statically
ifndef HAVE_WIN32
FFMPEGCONF += \
          --enable-libspeex \
          --enable-libopus \
          --enable-encoder=libspeex \
          --enable-decoder=libspeex
endif

DEPS_ffmpeg = iconv zlib x264 vpx opus speex $(DEPS_vpx)

ifdef HAVE_CROSS_COMPILE
FFMPEGCONF += --cross-prefix=$(HOST)-
endif

# x86 stuff
ifeq ($(ARCH),i386)
FFMPEGCONF += --arch=x86
endif

# Windows
ifdef HAVE_WIN32
FFMPEGCONF += --target-os=mingw32 --enable-memalign-hack
FFMPEGCONF += --enable-w32threads --disable-decoder=dca
endif

ifeq ($(call need_pkg,"ffmpeg >= 2.6.1"),)
PKGS_FOUND += ffmepg
endif

$(TARBALLS)/ffmpeg-github.tar.xz:
	$(call download_git,$(FFMPEG_GITURL),master)

.sum-ffmpeg: ffmpeg-github.tar.xz
	$(warning Not implemented.)
	touch $@

ffmpeg: ffmpeg-github.tar.xz .sum-ffmpeg
	rm -Rf $@ $@-github
	mkdir -p $@-github
	(cd $@-github && tar xv --strip-components=1 -f ../$<)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.ffmpeg: ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
