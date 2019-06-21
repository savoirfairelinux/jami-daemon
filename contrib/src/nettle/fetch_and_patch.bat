set BUILD=%SRC%..\build

set NETTLE_VERSION=c180b4d7afbda4049ad265d1366567f62a7a4a3a
set NETTLE_URL=https://github.com/ShiftMediaProject/nettle/archive/%NETTLE_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%NETTLE_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %NETTLE_URL%
)

7z -y x %NETTLE_VERSION%.tar.gz && 7z -y x %NETTLE_VERSION%.tar -o%BUILD%
del %NETTLE_VERSION%.tar && del %NETTLE_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\nettle-%NETTLE_VERSION% nettle

cd %SRC%
