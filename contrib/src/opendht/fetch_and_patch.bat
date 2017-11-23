set BUILD=%SRC%..\build

set OPENDHT_VERSION=2f6b47d985509d5e6c096b147b5ba7eb2d6e918e
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
