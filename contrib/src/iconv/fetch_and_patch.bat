set BUILD=%SRC%..\build

set ICONV_VERSION=a4d13b43f8bfc328a9d1d326b0d748d5236613be
set ICONV_URL=https://github.com/ShiftMediaProject/libiconv/archive/%ICONV_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%ICONV_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %ICONV_URL%
)

7z -y x %ICONV_VERSION%.tar.gz && 7z -y x %ICONV_VERSION%.tar -o%BUILD%
del %ICONV_VERSION%.tar && del %ICONV_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\libiconv-%ICONV_VERSION% iconv

cd %SRC%