set BUILD=%SRC%..\build

set OPENDHT_VERSION=ef82652a1e9417cd20df3bf55f569aee26b6e6a8
set OPENDHT_URL=https://github.com/savoirfairelinux/opendht/archive/%OPENDHT_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%OPENDHT_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %OPENDHT_URL%
)

7z -y x %OPENDHT_VERSION%.tar.gz && 7z -y x %OPENDHT_VERSION%.tar -o%BUILD%
del %OPENDHT_VERSION%.tar && del %OPENDHT_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\opendht-%OPENDHT_VERSION% opendht

cd %BUILD%\opendht

cd %SRC%
