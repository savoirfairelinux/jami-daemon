set BUILD=%SRC%..\build

mkdir %BUILD%
cd %BUILD%

set FFMPEG_VERSION=59da9dcd7ef6277e4e04998ced71b05a6083c635
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

for /F "tokens=* usebackq" %%F in (`bash -c "pwd | grep /mnt/c/"`) do (
    set NO_AUTO=%%F
)
if "%NO_AUTO%"=="" (
    set ROOTPATH=/c/
) else (
    set ROOTPATH=/mnt/c/
)
set UNIXPATH=%SRC:\=/%
set UNIXPATH=%ROOTPATH%%UNIXPATH:C:/=%
bash -c "%PATCH_CMD% %UNIXPATH%ffmpeg/change-RTCP-ratio.patch"
bash -c "%PATCH_CMD% %UNIXPATH%ffmpeg/rtp_ext_abs_send_time.patch"

git apply --reject --whitespace=fix %SRC%\ffmpeg\windows-configure.patch
git apply --reject --whitespace=fix %SRC%\ffmpeg\windows-configure-ffnvcodec.patch
git apply --reject --whitespace=fix %SRC%\ffmpeg\windows-configure-libmfx.patch

cd %SRC%