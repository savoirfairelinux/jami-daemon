#!/bin/bash
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
echo "configure and make ffmpeg for UWP-x64..."
cd $DIR/../../build/ffmpeg
rm -rf Output/Windows10/x64
mkdir -p Output/Windows10/x64
cd Output/Windows10/x64
../../../configure \
--toolchain=msvc \
--enable-gpl \
--disable-programs \
--disable-d3d11va \
--disable-dxva2 \
--arch=x86_64 \
--enable-shared \
--enable-cross-compile \
--target-os=win32 \
--enable-libopus \
--enable-encoder=libopus \
--enable-decoder=libopus \
--enable-encoder=libx264 \
--enable-decoder=h264 \
--enable-parser=h264 \
--enable-libx264 \
--extra-cflags="-MD -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00 -I../../../../../msvc/include -I../../../../../msvc/include/opus" \
--extra-ldflags="-APPCONTAINER WindowsApp.lib libopus.lib -LIBPATH:../../../../../msvc/lib/x64" \
--prefix=../../../Build/Windows10/x64
make -j8
make install
cd ../../..