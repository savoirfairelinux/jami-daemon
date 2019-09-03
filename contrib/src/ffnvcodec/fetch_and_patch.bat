set BUILD=%SRC%..\build

mkdir %BUILD%
cd %BUILD%

set FFNVCODEC_VERSION=5eeca8cc95267d55030e98a051effa47c45f13f3
set FFNVCODEC_GITURL=https://github.com/FFmpeg/nv-codec-headers/archive/%FFNVCODEC_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%FFNVCODEC_VERSION%.tar.gz %cd%
) else (
    wget --no-check-certificate %FFNVCODEC_GITURL%
)

7z -y x %FFNVCODEC_VERSION%.tar.gz && 7z -y x %FFNVCODEC_VERSION%.tar
del %FFNVCODEC_VERSION%.tar && del %FFNVCODEC_VERSION%.tar.gz && del pax_global_header
rename nv-codec-headers-%FFNVCODEC_VERSION% ffnvcodec

mkdir ..\msvc\include
mkdir ..\msvc\include\ffnvcodec
cd ffnvcodec\include\ffnvcodec
xcopy /S /Y *.h ..\..\..\..\msvc\include\ffnvcodec

cd %SRC%

