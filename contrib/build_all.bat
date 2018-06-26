@echo off
setlocal EnableDelayedExpansion

set SRC=%~dp0

set MSBUILD_ARGS=/nologo /p:useenv=true /p:Configuration=Release /p:Platform=x64 /verbosity:normal /maxcpucount:%NUMBER_OF_PROCESSORS%

@setlocal

set VSInstallerFolder="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if %PROCESSOR_ARCHITECTURE%==x86 set VSInstallerFolder="%ProgramFiles%\Microsoft Visual Studio\Installer"

pushd %VSInstallerFolder%
for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VSLATESTDIR=%%i
)
popd

echo VS Installation folder: %VSLATESTDIR%

if not exist "%VSLATESTDIR%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo:
    echo VSInstallDir not found or not installed correctly.
    goto cleanup
)

if %PROCESSOR_ARCHITECTURE%==x86 (
    set Comp_x86=x86 uwp 10.0.15063.0
    set Comp_x64=x86_amd64 uwp 10.0.15063.0
    set Comp_ARM=x86_arm uwp 10.0.15063.0
) else (
    set Comp_x86=amd64_x86 uwp 10.0.15063.0
    set Comp_x64=amd64 uwp 10.0.15063.0
    set Comp_ARM=amd64_arm uwp 10.0.15063.0
)

set MSYS2_PATH_TYPE=inherit

if not defined MSYS2_BIN (
    if exist C:\msys64\usr\bin\bash.exe set MSYS2_BIN="C:\msys64\usr\bin\bash.exe"
)
if not defined MSYS2_BIN (
    if exist C:\msys\usr\bin\bash.exe set MSYS2_BIN="C:\msys\usr\bin\bash.exe"
)

echo building ffmpeg for uwp x64...
set path=%path:"=%
call "%VSLATESTDIR%"\\VC\\Auxiliary\\Build\\vcvarsall.bat %Comp_x64%

rem * build libx264 *
call :build build\x264\SMP\libx264.vcxproj

rem * build libopus *
call :build build\opus\SMP\libopus.vcxproj

rem * build ffmpeg *
%MSYS2_BIN% --login -x %~dp0src/ffmpeg/windows-uwp-x64-configure-make.sh

rem * build openssl *
cd build\restbed\dependency\openssl
call perl Configure no-asm no-hw no-dso VC-WINUNIVERSAL
call ms\do_winuniversal
call ms\setVSvars universal10.0x64
call nmake -f ms\ntdll.mak
set PATH=restbed\dependency\openssl\out32dll;%PATH%

rem * build restbed w/asio *
cd ..\..
mkdir build
cd build
setlocal
set PATH=%ProgramFiles%\\CMake\\bin\\;%PATH%
cmake -DBUILD_SSL=ON -G "Visual Studio 15 2017 Win64" ..
cmake --build . --target ALL_BUILD --config Release
endlocal
cd ..\..

set DEPENDENCIES=( ^
build\jsoncpp\makefiles\vs2017\lib_json.vcxproj, ^
build\argon2\vs2015\Argon2Ref\Argon2Ref.vcxproj, ^
build\gmp\SMP\libgmp.vcxproj, ^
build\iconv\SMP\libiconv.vcxproj, ^
build\zlib\SMP\libzlib.vcxproj, ^
build\nettle\SMP\libnettle.vcxproj, ^
build\nettle\SMP\libhogweed.vcxproj, ^
build\gnutls\SMP\libgnutls.vcxproj, ^
build\msgpack-c\vs2017\msgpackc-static.vcxproj, ^
build\opendht\MSVC\opendht_vs2017.vcxproj, ^
build\pjproject\pjlib-util\build\pjlib_util.vcxproj, ^
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
build\pthreads\MSVC\pthreads.vcxproj, ^
build\libupnp\build\vs2017\ixml.vcxproj, ^
build\libupnp\build\vs2017\threadutil.vcxproj, ^
build\libupnp\build\vs2017\libupnp.vcxproj, ^
build\secp256k1\MSVC\secp256k1.vcxproj, ^
build\portaudio\msvc\uwp\portaudio.vcxproj, ^
build\yaml-cpp\msvc\yaml-cpp.vcxproj, ^
build\pcre\msvc\pcre.vcxproj, ^
build\libsamplerate\msvc\libsamplerate.vcxproj, ^
)

rem * build the rest *
for %%I in %DEPENDENCIES% do (
	call :build "%SRC%%%I"
)

:cleanup
endlocal
@endlocal
exit /B %ERRORLEVEL%

:build
echo "building " %*
msbuild %* %MSBUILD_ARGS%
exit /B 0