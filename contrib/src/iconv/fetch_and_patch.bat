set BUILD=%SRC%..\build

set ICONV_VERSION=73736371568e6976fa550c06d119fbbce7db8a15
set ICONV_URL=https://github.com/ShiftMediaProject/libiconv/archive/%ICONV_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%ICONV_VERSION%.tar.gz %cd%
) else (
    wget %ICONV_URL%
)

7z -y x %ICONV_VERSION%.tar.gz && 7z -y x %ICONV_VERSION%.tar -o%BUILD%
del %ICONV_VERSION%.tar && del %ICONV_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\libiconv-%ICONV_VERSION% iconv

cd %SRC%