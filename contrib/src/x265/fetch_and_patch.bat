set BUILD=%SRC%..\build

set H265_VERSION=3.1.1
set H265_URL=http://ftp.videolan.org/pub/videolan/x265/x265_%H265_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%H265_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %H265_URL%
)

7z -y x %H265_VERSION%.tar.gz && 7z -y x %H265_VERSION%.tar -o%BUILD%
del %H265_VERSION%.tar && del %H265_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\h265-%H265_VERSION% h265

cd %SRC%