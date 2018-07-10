@echo off
@setlocal EnableDelayedExpansion

set SRC=%~dp0

set platform=win32
set arch=64
if "%1"=="uwp" (
    set platform=x86
    if "%2"=="x86" (
        set arch=x86
        goto arch_x86
    ) else if "%2"=="x64" (
        set arch=x64
        goto arch_x64
    ) else (
        goto parameterError
    )
) else if "%1"=="win32" (
    if "%2"=="x86" (
        set arch=x86
        goto arch_x86
    ) else if "%2"=="x64" (
        set arch=x64
        goto arch_x64
    ) else (
        goto parameterError
    )
) else (
    goto parameterError
)

:arch_x86
set MSBUILD_ARGS=/nologo /p:useenv=true /p:Configuration=Release /p:Platform=Win32 /verbosity:normal /maxcpucount:%NUMBER_OF_PROCESSORS%
goto uwpProjs

:arch_x64
set MSBUILD_ARGS=/nologo /p:useenv=true /p:Configuration=Release /p:Platform=x64 /verbosity:normal /maxcpucount:%NUMBER_OF_PROCESSORS%
goto uwpProjs

:uwpProjs
set TOBUILD=( ^
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

:win32Projs
set TOBUILD=( ^
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
build\portaudio\msvc\portaudio.vcxproj, ^
build\yaml-cpp\msvc\yaml-cpp.vcxproj, ^
build\pcre\msvc\pcre.vcxproj, ^
build\libsamplerate\msvc\libsamplerate.vcxproj, ^
build\sndfile\msvc\libsndfile.vcxproj, ^
)

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
    set Comp_x86=x86 10.0.15063.0
    set Comp_x64=x86_amd64 10.0.15063.0
) else (
    set Comp_x86=amd64_x86 10.0.15063.0
    set Comp_x64=amd64 10.0.15063.0
)

set path=%path:"=%
if "%2"=="x86" (
    call "%VSLATESTDIR%"\\VC\\Auxiliary\\Build\\vcvarsall.bat %Comp_x86%
) else if "%2"=="x64" (
    call "%VSLATESTDIR%"\\VC\\Auxiliary\\Build\\vcvarsall.bat %Comp_x64%
)

set MSYS2_PATH_TYPE=inherit

if not defined MSYS2_BIN (
    if exist C:\msys64\usr\bin\bash.exe set MSYS2_BIN="C:\msys64\usr\bin\bash.exe"
)
if not defined MSYS2_BIN (
    if exist C:\msys\usr\bin\bash.exe set MSYS2_BIN="C:\msys\usr\bin\bash.exe"
)

if "%1"=="win32" (
        :: build livpx
        call :build build\vpx\SMP\libvpx.vcxproj
)
:: build libx264
call :build build\x264\SMP\libx264.vcxproj
:: build libopus
call :build build\opus\SMP\libopus.vcxproj
:: build ffmpeg
%MSYS2_BIN% --login -x %~dp0src/ffmpeg/windows-configure-make.sh %1 %2

:: build openssl
cd build\restbed\dependency\openssl
if "%1"=="win32" (
    call perl Configure VC-WIN64A
    call ms\do_win64a
) else if "%1"=="uwp" (
    call perl Configure no-asm no-hw no-dso VC-WINUNIVERSAL
    call ms\do_winuniversal
    call ms\setVSvars universal10.0x64
)
call nmake -f ms\ntdll.mak
set PATH=restbed\dependency\openssl\out32dll;%PATH%

rem * build restbed w/asio *
cd ..\..
mkdir build
cd build
setlocal
set PATH=C:\\Program Files\\CMake\\bin\\;%PATH%
if "%2"=="x86" (
    cmake -DBUILD_SSL=ON -G "Visual Studio 15 2017 Win32" ..
) else if "%2"=="x64" (
    cmake -DBUILD_SSL=ON -G "Visual Studio 15 2017 Win64" ..
)
cmake --build . --target ALL_BUILD --config Release
cd ..\..

::build the list
for %%I in %TOBUILD% do (
    call :build "%SRC%%%I"
)

:parameterError
echo "parameter error"
goto cleanup

:cleanup
endlocal
@endlocal
exit /B %ERRORLEVEL%

:build
echo "building " %*
msbuild %* %MSBUILD_ARGS%
exit /B 0