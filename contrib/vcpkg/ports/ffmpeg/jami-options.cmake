# Jami component selection. Mirrors contrib/src/ffmpeg/{rules.mak,windows-configure-make.sh}.
# Library-level toggles (libopus, libx264, libvpx, nvcodec, qsv, gpl, ...) are
# driven by vcpkg features; only ffmpeg *components* are listed here.
# --disable-everything must precede every --enable-<component>, so it is prepended.

set(JAMI_OPTIONS
    --enable-protocols
    --enable-bsfs
    --enable-demuxers
    --enable-muxers
    --disable-filters
    # parsers
    --enable-parser=h263 --enable-parser=h264 --enable-parser=hevc
    --enable-parser=mpeg4video --enable-parser=vp8 --enable-parser=vp9 --enable-parser=opus
    # audio codecs
    --enable-encoder=adpcm_g722 --enable-decoder=adpcm_g722
    --enable-encoder=pcm_alaw --enable-decoder=pcm_alaw
    --enable-encoder=pcm_mulaw --enable-decoder=pcm_mulaw
    # video codecs
    --enable-decoder=h264
    --enable-encoder=rawvideo --enable-decoder=rawvideo
    --enable-encoder=mpeg4 --enable-decoder=mpeg4
    --enable-encoder=h263 --enable-encoder=h263p --enable-decoder=h263
    --enable-encoder=mjpeg --enable-decoder=mjpeg --enable-decoder=mjpegb
    # ringtones / audio streaming
    --enable-decoder=flac --enable-decoder=vorbis --enable-decoder=aac
    --enable-decoder=ac3 --enable-decoder=eac3 --enable-decoder=mp3
    --enable-decoder=pcm_u24be --enable-decoder=pcm_u24le
    --enable-decoder=pcm_u32be --enable-decoder=pcm_u32le
    --enable-decoder=pcm_u8
    --enable-decoder=pcm_f16le --enable-decoder=pcm_f24le
    --enable-decoder=pcm_f32be --enable-decoder=pcm_f32le
    --enable-decoder=pcm_f64be --enable-decoder=pcm_f64le
    --enable-decoder=pcm_s16be --enable-decoder=pcm_s16be_planar
    --enable-decoder=pcm_s16le --enable-decoder=pcm_s16le_planar
    --enable-decoder=pcm_s24be --enable-decoder=pcm_s24le --enable-decoder=pcm_s24le_planar
    --enable-decoder=pcm_s32be --enable-decoder=pcm_s32le --enable-decoder=pcm_s32le_planar
    --enable-decoder=pcm_s64be --enable-decoder=pcm_s64le
    --enable-decoder=pcm_s8 --enable-decoder=pcm_s8_planar
    --enable-decoder=pcm_u16be --enable-decoder=pcm_u16le
    # images
    --enable-encoder=gif --enable-decoder=gif
    --enable-encoder=jpegls --enable-decoder=jpegls
    --enable-encoder=ljpeg --enable-decoder=jpeg2000
    --enable-encoder=png --enable-decoder=png
    --enable-encoder=bmp --enable-decoder=bmp
    --enable-encoder=tiff --enable-decoder=tiff
    # filters
    --enable-filter=scale --enable-filter=overlay
    --enable-filter=amix --enable-filter=amerge --enable-filter=aresample
    --enable-filter=format --enable-filter=aformat
    --enable-filter=fps --enable-filter=transpose --enable-filter=pad
)

if("opus" IN_LIST FEATURES)
    list(APPEND JAMI_OPTIONS --enable-encoder=libopus --enable-decoder=libopus)
endif()
if("x264" IN_LIST FEATURES)
    list(APPEND JAMI_OPTIONS --enable-encoder=libx264)
endif()
if("vpx" IN_LIST FEATURES)
    list(APPEND JAMI_OPTIONS --enable-encoder=libvpx_vp8 --enable-decoder=vp8 --enable-decoder=vp9)
endif()
if("nvcodec" IN_LIST FEATURES)
    list(APPEND JAMI_OPTIONS
        --enable-hwaccel=h264_nvdec --enable-hwaccel=hevc_nvdec
        --enable-hwaccel=vp8_nvdec --enable-hwaccel=mjpeg_nvdec
        --enable-encoder=h264_nvenc --enable-encoder=hevc_nvenc)
endif()
if("qsv" IN_LIST FEATURES)
    list(APPEND JAMI_OPTIONS
        --enable-encoder=h264_qsv --enable-encoder=hevc_qsv --enable-encoder=mjpeg_qsv
        --enable-decoder=vp8_qsv --enable-decoder=h264_qsv --enable-decoder=hevc_qsv
        --enable-decoder=mjpeg_qsv --enable-decoder=vp9_qsv
        --enable-filter=scale_qsv --enable-filter=overlay_qsv)
endif()

if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_UWP)
    list(APPEND JAMI_OPTIONS
        --enable-indev=dshow --enable-indev=gdigrab --enable-indev=dxgigrab
        --enable-dxva2)
    if(VCPKG_DETECTED_MSVC)
        # dxgigrab is C++/WinRT and needs the Win10 API surface; configure
        # forces -D_WIN32_WINNT=0x0600 for msvc, so override it for C++ only.
        list(APPEND JAMI_OPTIONS
            "--extra-cxxflags=-std:c++20" "--extra-cxxflags=/Zc:__cplusplus"
            "--extra-cxxflags=-D_WIN32_WINNT=0x0A00")
    endif()
endif()

list(JOIN JAMI_OPTIONS " " JAMI_OPTIONS_STR)
set(OPTIONS "--disable-everything ${OPTIONS} ${JAMI_OPTIONS_STR}")
