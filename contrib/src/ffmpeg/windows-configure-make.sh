#!/bin/bash
set +x
set +e
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $DIR/../../build/ffmpeg
FFMPEGCONF='
            --toolchain=msvc
            --target-os=win32'
                
FFMPEGCONF+='
            --disable-everything
            --disable-programs
            --disable-d3d11va
            --disable-dxva2
            --disable-postproc
            --disable-filters'

FFMPEGCONF+='
            --enable-shared
            --enable-cross-compile
            --enable-gpl
            --enable-swscale
            --enable-protocols
            --enable-bsfs'

FFMPEGCONF+='
            --enable-demuxers
            --enable-muxers'

FFMPEGCONF+='
            --enable-parser=h263
            --enable-parser=h264
            --enable-parser=mpeg4video
            --enable-parser=vp8
            --enable-parser=vp9
            --enable-parser=opus'

FFMPEGCONF+='
            --enable-libopus
            --enable-encoder=libopus
            --enable-decoder=libopus
            --enable-encoder=adpcm_g722
            --enable-decoder=adpcm_g722
            --enable-encoder=pcm_alaw
            --enable-decoder=pcm_alaw
            --enable-encoder=pcm_mulaw
            --enable-decoder=pcm_mulaw'

FFMPEGCONF+='
            --enable-libx264
            --enable-encoder=libx264
            --enable-decoder=h264
            --enable-encoder=rawvideo
            --enable-decoder=rawvideo
            --enable-encoder=mpeg4
            --enable-decoder=mpeg4
            --enable-encoder=h263
            --enable-encoder=h263p
            --enable-decoder=h263
            --enable-encoder=mjpeg
            --enable-decoder=mjpeg
            --enable-decoder=mjpegb'

if [ "$1" == "uwp" ]; then
    FFMPEGCONF+='
                --disable-network
                --extra-cflags="-MD -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00 -I../../../../../msvc/include -I../../../../../msvc/include/opus"'
    if [ "$2" == "x64" ]; then
        echo "configure and make ffmpeg for UWP-x64..."
            FFMPEGCONF+='
                        --arch=x86_64
                        --extra-ldflags="-APPCONTAINER WindowsApp.lib libopus.lib libx264.lib -LIBPATH:../../../../../msvc/lib/x64"
                        --prefix=../../../Build/Windows10/x64'
            OUTDIR=Output/Windows10/x64
    elif [ "$2" == "x86" ]; then
        echo "configure and make ffmpeg for UWP-x86..."
            FFMPEGCONF+='
                        --arch=x86
                        --extra-ldflags="-APPCONTAINER WindowsApp.lib libopus.lib libx264.lib -LIBPATH:../../../../../msvc/lib/x86"
                        --prefix=../../../Build/Windows10/x86'
            OUTDIR=Output/Windows10/x86
    fi
elif [ "$1" == "win32" ]; then
    EXTRACFLAGS='-MD -D_WINDLL -I../../../../../msvc/include -I../../../../../msvc/include/opus -I../../../../../msvc/include/vpx'
    FFMPEGCONF+='
                --enable-libvpx
                --enable-encoder=libvpx_vp8
                --enable-decoder=vp8
                --enable-decoder=vp9'
    FFMPEGCONF+='
                --enable-indev=dshow
                --enable-indev=gdigrab
                --enable-dxva2'
    if [ "$2" == "x64" ]; then
        echo "configure and make ffmpeg for win32-x64..."
        EXTRALDFLAGS='-APPCONTAINER:NO -MACHINE:x64 Ole32.lib Kernel32.lib Gdi32.lib User32.lib Strmiids.lib OleAut32.lib Shlwapi.lib Vfw32.lib Secur32.lib libopus.lib libx264.lib libvpx.lib -LIBPATH:../../../../../msvc/lib/x64'
        FFMPEGCONF+=' --arch=x86_64'
        PREFIX=../../../Build/win32/x64
        OUTDIR=Output/win32/x64
    elif [ "$2" == "x86" ]; then
        echo "configure and make ffmpeg for win32-x86..."
        EXTRALDFLAGS='-APPCONTAINER:NO -MACHINE:x86 Ole32.lib Kernel32.lib Gdi32.lib User32.lib Strmiids.lib OleAut32.lib Shlwapi.lib Vfw32.lib Secur32.lib libopus.lib libx264.lib libvpx.lib -LIBPATH:../../../../../msvc/lib/x86'
        FFMPEGCONF+=' --arch=x86'
        PREFIX=../../../Build/win32/x86
        OUTDIR=Output/win32/x86
    fi
fi
rm -rf $OUTDIR
mkdir -p $OUTDIR
cd $OUTDIR
pwd
FFMPEGCONF=$(echo $FFMPEGCONF | sed -e "s/[[:space:]]\+/ /g")
set -x
set -e
../../../configure $FFMPEGCONF --extra-cflags="${EXTRACFLAGS}" --extra-ldflags="${EXTRALDFLAGS}" --prefix="${PREFIX}"
make -j8 install
cd ../../..