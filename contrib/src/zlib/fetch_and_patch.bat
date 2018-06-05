set BUILD=%SRC%..\build

set ZLIB_VERSION=8e4e3ead55cdd296130242d86b44b92fde3ea4d4
set ZLIB_URL=https://github.com/ShiftMediaProject/zlib/archive/%ZLIB_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%ZLIB_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %ZLIB_URL%
)

7z -y x %ZLIB_VERSION%.tar.gz && 7z -y x %ZLIB_VERSION%.tar -o%BUILD%
del %ZLIB_VERSION%.tar && del %ZLIB_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\zlib-%ZLIB_VERSION% zlib

cd %SRC%