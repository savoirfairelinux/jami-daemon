@echo on
SETLOCAL EnableDelayedExpansion

set SRC=%~dp0

set PATH=%PATH%;%ProgramFiles(x86)%\MSBuild\14.0\Bin\

set MSBUILD_ARGS=/nologo /p:Configuration=Release /p:Platform=x64 /verbosity:normal /maxcpucount:%NUMBER_OF_PROCESSORS%

set DEPENDENCIES=( ^
build\argon2\vs2015\Argon2Ref\Argon2Ref.vcxproj, ^
build\boost\MSVC\random\random.vcxproj, ^
build\boost\MSVC\system\system.vcxproj, ^
build\jsoncpp\makefiles\msvc2010\lib_json.vcxproj, ^
build\cryptopp\cryptlib.vcxproj, ^
build\opendht\MSVC\argon.vcxproj, ^
build\opendht\MSVC\blake.vcxproj, ^
build\gmp\SMP\libgmp.vcxproj, ^
build\nettle\SMP\libnettle.vcxproj, ^
build\nettle\SMP\libhogweed.vcxproj, ^
build\libiconv\SMP\libiconv.vcxproj, ^
build\nettle\SMP\libiconv.vcxproj, ^
build\zlib\SMP\libzlib.vcxproj, ^
build\gnutls\SMP\libgnutls.vcxproj, ^
build\msgpack-c\msgpack_vc8.vcxproj, ^
build\opendht\MSVC\opendht.vcxproj, ^
build\libsamplerate\MSVC\libsamplerate.vcxproj, ^
build\pthreads\MSVC\pthreads-UWP-S\pthreads-UWP-S.vcxproj, ^
build\libupnp\build\VS2015\ixml.vcxproj, ^
build\libupnp\build\VS2015\threadutil.vcxproj, ^
build\libupnp\build\VS2015\libupnp.vcxproj, ^
build\pcre\MSVC\pcre.vcxproj, ^
build\pjproject\third_party\build\baseclasses\libbaseclasses.vcxproj, ^
build\pjproject\third_party\build\g7221\libg7221codec.vcxproj, ^
build\pjproject\third_party\build\gsm\libgsmcodec.vcxproj, ^
build\pjproject\third_party\build\ilbc\libilbccodec.vcxproj, ^
build\pjproject\third_party\build\milenage\libmilenage.vcxproj, ^
build\pjproject\third_party\build\resample\libresample.vcxproj, ^
build\pjproject\third_party\build\speex\libspeex.vcxproj, ^
build\pjproject\third_party\build\srtp\libsrtp.vcxproj, ^
build\pjproject\third_party\build\yuv\libyuv.vcxproj, ^
build\pjproject\pjlib-util\build\pjlib-util.vcxproj, ^
build\pjproject\pjmedia\build\pjmedia.vcxproj, ^
build\pjproject\pjmedia\build\pjmedia_audiodev.vcxproj, ^
build\pjproject\pjmedia\build\pjmedia_codec.vcxproj, ^
build\pjproject\pjmedia\build\pjmedia_videodev.vcxproj, ^
build\pjproject\pjlib\build\pjlib.vcxproj, ^
build\pjproject\pjsip\build\pjsip_core.vcxproj, ^
build\pjproject\pjsip\build\pjsip_simple.vcxproj, ^
build\pjproject\pjsip\build\pjsua_lib.vcxproj, ^
build\pjproject\pjsip\build\pjsua2_lib.vcxproj, ^
build\pjproject\pjsip\build\pjsip_ua.vcxproj, ^
build\pjproject\pjnath\build\pjnath.vcxproj, ^
build\pjproject\pjsip-apps\build\libpjproject.vcxproj, ^
build\portaudio\MSVC\portaudio-UWP\portaudio-UWP.vcxproj, ^
build\yaml-cpp\MSVC\yaml-cpp.vcxproj, ^
)

rem * build libx264 *
call :build build\x264\SMP\libx264.vcxproj
rem * build libopus *
call :build build\opus\SMP\libopus.vcxproj
rem * build ffmpeg *
cd build\FFmpegInterop
SET LIB="%VSINSTALLDIR%VC\lib\store\amd64;%VSINSTALLDIR%VC\atlmfc\lib\amd64;%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\x64;;%UniversalCRTSdkDir%lib\%UCRTVersion%\um\x64;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\lib\um\x64;;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Lib\um\x64"
SET LIBPATH="%VSINSTALLDIR%VC\atlmfc\lib\amd64;%VSINSTALLDIR%VC\lib\amd64;"
SET INCLUDE="%VSINSTALLDIR%VC\include;%VSINSTALLDIR%VC\atlmfc\include;%UniversalCRTSdkDir%Include\%UCRTVersion%\ucrt;%UniversalCRTSdkDir%Include\%UCRTVersion%\um;%UniversalCRTSdkDir%Include\%UCRTVersion%\shared;%UniversalCRTSdkDir%Include\%UCRTVersion%\winrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Include\um;"
set MSYS2_BIN="C:\msys64\usr\bin\bash.exe"
call BuildFFmpeg.bat win10 x64
cd ..\..

rem * build openssl UWP *
cd build\restbed\dependency\openssl
call perl Configure no-asm no-hw no-dso VC-WINUNIVERSAL
call ms\do_winuniversal
call ms\setVSvars universal10.0x64
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
call nmake -f ms\ntdll.mak
set PATH=restbed\dependency\openssl\out32dll;%PATH%

rem * build restbed w/asio *
cd ..\..
mkdir build
cd build
cmake -G "Visual Studio 14 2015 Win64" ..
cmake --build . --target ALL_BUILD --config Release
cd ..\..

rem * build the rest *
for %%I in %DEPENDENCIES% do (
    call :build "%SRC%%%I"
)

exit /B %ERRORLEVEL%

:build
echo "Building project: " %*
msbuild %* %MSBUILD_ARGS%
exit /B 0