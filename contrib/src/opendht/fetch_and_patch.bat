set BUILD=%SRC%..\build

set OPENDHT_VERSION=4d4ba2be96ae237f21e65acdc3eab24d7d5d0e00
set OPENDHT_URL=https://github.com/savoirfairelinux/opendht/archive/%OPENDHT_VERSION%.tar.gz

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
git apply --reject --whitespace=fix %SRC%\opendht\opendht-proxy-uwp.patch

cd %SRC%
