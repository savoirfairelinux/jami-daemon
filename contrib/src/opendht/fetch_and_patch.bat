set BUILD=%SRC%..\build

set OPENDHT_VERSION=88e519fd0bb485f2a1b08e5b2162c7436e4a019a
set OPENDHT_URL=https://github.com/AmarOk1412/opendht/archive/%OPENDHT_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%OPENDHT_VERSION%.tar.gz %cd%
) else (
    wget %OPENDHT_URL%
)

7z -y x %OPENDHT_VERSION%.tar.gz && 7z -y x %OPENDHT_VERSION%.tar -o%BUILD%
del %OPENDHT_VERSION%.tar && del %OPENDHT_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\opendht-%OPENDHT_VERSION% opendht

cd %BUILD%\opendht

git apply --reject --whitespace=fix %SRC%\opendht\opendht-uwp.patch

cd %SRC%
