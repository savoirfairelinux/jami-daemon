set BUILD=%SRC%..\build

set FFMPEGINTEROP_VERSION=10879040235da3a2393ff19a7a6dc3d7f791314a
set FFMPEGINTEROP_URL=https://github.com/Microsoft/FFmpegInterop/archive/%FFMPEGINTEROP_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%FFMPEGINTEROP_VERSION%.tar.gz %cd%
) else (
    wget %FFMPEGINTEROP_URL%
)

7z -y x %FFMPEGINTEROP_VERSION%.tar.gz && 7z -y x %FFMPEGINTEROP_VERSION%.tar -o%BUILD%
del %FFMPEGINTEROP_VERSION%.tar && del %FFMPEGINTEROP_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\FFmpegInterop-%FFMPEGINTEROP_VERSION% FFmpegInterop

cd %BUILD%\FFmpegInterop

rmdir /s /q ffmpeg

set FFMPEG_VERSION=6d7192bcb7bbab17dc194e8dbb56c208bced0a92
set FFMPEG_URL=https://github.com/FFmpeg/FFmpeg/archive/%FFMPEG_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%FFMPEG_VERSION%.tar.gz %cd%
) else (
    wget %FFMPEG_URL%
)

7z -y x %FFMPEG_VERSION%.tar.gz && 7z -y x %FFMPEG_VERSION%.tar
del %FFMPEG_VERSION%.tar && del %FFMPEG_VERSION%.tar.gz && del pax_global_header
rename FFmpeg-%FFMPEG_VERSION% ffmpeg

git apply --reject --whitespace=fix %SRC%\ffmpeg\ffmpeg-uwp.patch

cd %SRC%