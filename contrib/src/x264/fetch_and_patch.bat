set BUILD=%SRC%..\build

set X264_VERSION=5fee86cae91cd7b726db7408a3ed1c4da71fb78c
set X264_URL=https://github.com/ShiftMediaProject/x264/archive/%X264_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%X264_VERSION%.tar.gz %cd%
) else (
    wget %X264_URL%
)

7z -y x %X264_VERSION%.tar.gz && 7z -y x %X264_VERSION%.tar -o%BUILD%
del %X264_VERSION%.tar && del %X264_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\x264-%X264_VERSION% x264

cd %BUILD%\x264

rem git apply --reject --whitespace=fix %SRC%\x264\x264-uwp.patch

cd %SRC%