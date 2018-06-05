set BUILD=%SRC%..\build

mkdir %BUILD%
cd %BUILD%

set FFMPEG_VERSION=5ee203076fa1b1b5da32f525f2b6df3bd5e93b09
set FFMPEG_URL=https://github.com/FFmpeg/FFmpeg/archive/%FFMPEG_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%FFMPEG_VERSION%.tar.gz %cd%
) else (
    wget --no-check-certificate %FFMPEG_URL%
)

7z -y x %FFMPEG_VERSION%.tar.gz && 7z -y x %FFMPEG_VERSION%.tar
del %FFMPEG_VERSION%.tar && del %FFMPEG_VERSION%.tar.gz && del pax_global_header
rename FFmpeg-%FFMPEG_VERSION% ffmpeg

cd ffmpeg
git apply --reject --whitespace=fix %SRC%\ffmpeg\windows-no-pkg-config.patch

cd %SRC%