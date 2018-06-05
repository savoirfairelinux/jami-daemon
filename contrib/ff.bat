@echo on
setlocal EnableDelayedExpansion

set SRC=%~dp0

set VCInstallDir=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\
set UCRTVersion=10.0.14393.0
set MSVCVersion=14.14.26428
set UniversalCRTSdkDir=C:\Program Files (x86)\Windows Kits\10\
call "%VCInstallDir%Auxiliary\Build\vcvarsall.bat" x64 store

set MSBUILD_ARGS=/nologo /p:Configuration=Release /p:Platform=x64 /verbosity:normal /maxcpucount:%NUMBER_OF_PROCESSORS%

set MSYS2_PATH_TYPE=inherit
set MSYS2_BIN="C:\msys64\usr\bin\bash.exe"

rem * build libx264 *
call :build build\x264\SMP\libx264.vcxproj

rem * build libopus *
call :build build\opus\SMP\libopus.vcxproj

rem * build ffmpeg *
set INCLUDE=%VCInstallDir%Tools\MSVC\%MSVCVersion%\atlmfc\include;%VCInstallDir%Tools\MSVC\%MSVCVersion%\include;%UniversalCRTSdkDir%include\%UCRTVersion%\ucrt;%UniversalCRTSdkDir%include\%UCRTVersion%\shared;%UniversalCRTSdkDir%include\%UCRTVersion%\um;%UniversalCRTSdkDir%include\%UCRTVersion%\winrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Include\um;
set LIBPATH=%VCInstallDir%Tools\MSVC\%MSVCVersion%\atlmfc\lib\x64;%VCInstallDir%Tools\MSVC\%MSVCVersion%\lib\x64;%UniversalCRTSdkDir%UnionMetadata;%UniversalCRTSdkDir%References;
set LIB=%VCInstallDir%Tools\MSVC\%MSVCVersion%\atlmfc\lib\x64;%VCInstallDir%Tools\MSVC\%MSVCVersion%\lib\x64;%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\x64;%UniversalCRTSdkDir%lib\%UCRTVersion%\um\x64;

%MSYS2_BIN% --login %~dp0src/ffmpeg/windows-uwp-x64-configure-make.sh
exit /B %ERRORLEVEL%

:build
echo "Building project: " %*
msbuild %* %MSBUILD_ARGS%
exit /B 0